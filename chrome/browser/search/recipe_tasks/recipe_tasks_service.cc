// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/recipe_tasks/recipe_tasks_service.h"

#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {
const char kNewTabRecipeTasksApiPath[] = "/async/newtab_recipe_tasks";
const char kXSSIResponsePreamble[] = ")]}'";
const char kDismissedTasksPrefName[] = "NewTabPage.DismissedRecipeTasks";

GURL GetApiUrl(const std::string& application_locale) {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }
  return net::AppendQueryParameter(
      google_base_url.Resolve(kNewTabRecipeTasksApiPath), "hl",
      application_locale);
}
}  // namespace

RecipeTasksService::RecipeTasksService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const std::string& application_locale)
    : profile_(profile),
      url_loader_factory_(url_loader_factory),
      application_locale_(application_locale) {}

RecipeTasksService::~RecipeTasksService() = default;

// static
void RecipeTasksService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedTasksPrefName);
}

void RecipeTasksService::Shutdown() {}

void RecipeTasksService::GetPrimaryRecipeTask(RecipeTaskCallback callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("recipe_tasks_service", R"(
        semantics {
          sender: "Recipe Tasks Service"
          description: "This service downloads recipe tasks, which is "
            "information related to the user's currently active recipe "
            "search journeys such as visisted and recommended recipes. "
            "Recipe tasks will be displayed on the new tab page to help the "
            "user to continue their search journey. Recipe tasks are queried "
            "on every new tab page load."
          trigger:
            "Displaying the new tab page on Desktop, if Google is the "
            "configured search provider and the user is signed in."
          data: "Credentials if user is signed in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine' or by "
            "signing out."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetApiUrl(application_locale_);
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));
  variations::AppendVariationsHeaderUnknownSignedIn(
      resource_request->url,
      /* Modules are only shown in non-incognito. */
      variations::InIncognito::kNo, resource_request.get());

  loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation));
  loaders_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&RecipeTasksService::OnDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get(),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void RecipeTasksService::DismissRecipeTask(const std::string& task_name) {
  ListPrefUpdate update(profile_->GetPrefs(), kDismissedTasksPrefName);
  update->AppendIfNotPresent(std::make_unique<base::Value>(task_name));
}

void RecipeTasksService::RestoreRecipeTask(const std::string& task_name) {
  ListPrefUpdate update(profile_->GetPrefs(), kDismissedTasksPrefName);
  update->EraseListValue(base::Value(task_name));
}

void RecipeTasksService::OnDataLoaded(network::SimpleURLLoader* loader,
                                      RecipeTaskCallback callback,
                                      std::unique_ptr<std::string> response) {
  auto net_error = loader->NetError();
  base::EraseIf(loaders_, [loader](const auto& target) {
    return loader == target.get();
  });

  if (net_error != net::OK || !response) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (base::StartsWith(*response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    *response = response->substr(strlen(kXSSIResponsePreamble));
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&RecipeTasksService::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RecipeTasksService::OnJsonParsed(
    RecipeTaskCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    std::move(callback).Run(nullptr);
    return;
  }
  // We receive a list of recipe tasks ordered from highest to lowest
  // priority. We only support showing a single task though. Therefore, pick the
  // first task.
  auto* recipe_tasks = result.value->FindListPath("update.recipe_tasks");
  if (!recipe_tasks || recipe_tasks->GetList().size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }
  for (const auto& recipe_task : recipe_tasks->GetList()) {
    auto* title = recipe_task.FindStringPath("title");
    auto* task_name = recipe_task.FindStringPath("task_name");
    auto* recipes = recipe_task.FindListPath("recipes");
    auto* related_searches = recipe_task.FindListPath("related_searches");
    if (!title || !task_name || !recipes || !related_searches ||
        recipes->GetList().size() == 0) {
      continue;
    }
    if (IsRecipeTaskDismissed(*task_name)) {
      continue;
    }
    std::vector<recipe_tasks::mojom::RecipePtr> mojo_recipes;
    for (const auto& recipe : recipes->GetList()) {
      auto* name = recipe.FindStringPath("name");
      auto* image_url = recipe.FindStringPath("image_url");
      auto* info = recipe.FindStringPath("info");
      auto* target_url = recipe.FindStringPath("target_url");
      if (!name || !image_url || !info || !target_url) {
        continue;
      }
      auto mojo_recipe = recipe_tasks::mojom::Recipe::New();
      mojo_recipe->name = *name;
      mojo_recipe->image_url = GURL(*image_url);
      mojo_recipe->info = *info;
      mojo_recipe->target_url = GURL(*target_url);
      mojo_recipes.push_back(std::move(mojo_recipe));
    }
    std::vector<recipe_tasks::mojom::RelatedSearchPtr> mojo_related_searches;
    for (const auto& related_search : related_searches->GetList()) {
      auto* text = related_search.FindStringPath("text");
      auto* target_url = related_search.FindStringPath("target_url");
      if (!text || !target_url) {
        continue;
      }
      auto mojo_related_search = recipe_tasks::mojom::RelatedSearch::New();
      mojo_related_search->text = *text;
      mojo_related_search->target_url = GURL(*target_url);
      mojo_related_searches.push_back(std::move(mojo_related_search));
    }
    auto mojo_recipe_task = recipe_tasks::mojom::RecipeTask::New();
    mojo_recipe_task->title = *title;
    mojo_recipe_task->name = *task_name;
    mojo_recipe_task->recipes = std::move(mojo_recipes);
    mojo_recipe_task->related_searches = std::move(mojo_related_searches);
    std::move(callback).Run(std::move(mojo_recipe_task));
    return;
  }
  std::move(callback).Run(nullptr);
}

bool RecipeTasksService::IsRecipeTaskDismissed(const std::string& task_name) {
  const base::ListValue* dismissed_tasks =
      profile_->GetPrefs()->GetList(kDismissedTasksPrefName);
  DCHECK(dismissed_tasks);
  return dismissed_tasks->Find(base::Value(task_name)) !=
         dismissed_tasks->end();
}
