// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/recipes/recipes_service.h"

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/modules/recipes/time_format_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace {
const char kXSSIResponsePreamble[] = ")]}'";
constexpr char kPath[] = "/async/newtab_recipe_tasks";
constexpr char kDismissedTasksPrefName[] = "NewTabPage.DismissedRecipeTasks";

// We return a reference so that base::FeatureList::CheckFeatureIdentity
// succeeds.
const base::Feature& GetFeature() {
  return ntp_features::kNtpRecipeTasksModule;
}

const char* GetDataParam() {
  return ntp_features::kNtpRecipeTasksModuleDataParam;
}

const char* GetCacheMaxAgeSParam() {
  return ntp_features::kNtpRecipeTasksModuleCacheMaxAgeSParam;
}

const char* GetExperimentGroupParam() {
  return ntp_features::kNtpRecipeTasksModuleExperimentGroupParam;
}

GURL GetApiUrl(const std::string& application_locale) {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }
  auto url = net::AppendQueryParameter(google_base_url.Resolve(kPath), "hl",
                                       application_locale);
  if (base::GetFieldTrialParamValueByFeature(GetFeature(), GetDataParam()) ==
      "fake") {
    url = google_util::AppendToAsyncQueryParam(url, "fake_data", "1");
  }
  int cache_max_age_s = base::GetFieldTrialParamByFeatureAsInt(
      GetFeature(), GetCacheMaxAgeSParam(), 0);
  if (cache_max_age_s > 0) {
    url = google_util::AppendToAsyncQueryParam(
        url, "cache_max_age_s", base::NumberToString(cache_max_age_s));
  }
  auto experiment_group = base::GetFieldTrialParamValueByFeature(
      GetFeature(), GetExperimentGroupParam());
  if (!experiment_group.empty()) {
    url = google_util::AppendToAsyncQueryParam(url, "experiment_group",
                                               experiment_group);
  }
  return url;
}
}  // namespace

RecipesService::RecipesService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const std::string& application_locale)
    : profile_(profile),
      url_loader_factory_(url_loader_factory),
      application_locale_(application_locale) {}

RecipesService::~RecipesService() = default;

// static
void RecipesService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedTasksPrefName);
}

void RecipesService::Shutdown() {}

void RecipesService::GetPrimaryTask(RecipesCallback callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("recipes_service", R"(
        semantics {
          sender: "Recipe Module Service"
          description: "This service downloads recipes, which is information "
            "related to the user's currently active cooking recipe search "
            "journeys such as visited and recommended recipes."
            "Recipes will be displayed on the new tab page to help the user "
            "to continue their search journey. Recipes are queried on every "
            "new tab page load."
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
      base::BindOnce(&RecipesService::OnDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get(),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void RecipesService::DismissTask(const std::string& task_name) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), kDismissedTasksPrefName);
  base::Value::List& update_list = update.Get();
  base::Value task_name_value(task_name);
  if (!base::Contains(update_list, task_name_value))
    update_list.Append(std::move(task_name_value));
}

void RecipesService::RestoreTask(const std::string& task_name) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), kDismissedTasksPrefName);
  update->EraseValue(base::Value(task_name));
}

void RecipesService::OnDataLoaded(network::SimpleURLLoader* loader,
                                  RecipesCallback callback,
                                  std::unique_ptr<std::string> response) {
  auto net_error = loader->NetError();
  bool loaded_from_cache = loader->LoadedFromCache();
  base::EraseIf(loaders_, [loader](const auto& target) {
    return loader == target.get();
  });

  if (!loaded_from_cache) {
    base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                             base::PersistentHash("recipe_tasks"));
  }

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
      base::BindOnce(&RecipesService::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RecipesService::OnJsonParsed(
    RecipesCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // We receive a list of tasks ordered from highest to lowest priority. We only
  // support showing a single task though. Therefore, pick the first task.
  auto* tasks = result->GetDict().FindListByDottedPath("update.recipe_tasks");
  if (!tasks || tasks->size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }

  for (const auto& task : *tasks) {
    const base::Value::Dict& task_dict = task.GetDict();
    auto* title = task_dict.FindString("title");
    auto* task_name = task_dict.FindString("task_name");
    auto* recipes = task_dict.FindListByDottedPath("recipes");
    auto* related_searches = task_dict.FindList("related_searches");
    if (!title || !task_name || !recipes ||
        recipes->size() == 0) {
      continue;
    }
    if (IsTaskDismissed(*task_name)) {
      continue;
    }
    auto mojo_task = recipes::mojom::Task::New();
    std::vector<recipes::mojom::RecipePtr> mojo_recipes;
    for (const auto& recipe : *recipes) {
      const base::Value::Dict& recipe_dict = recipe.GetDict();
      const auto* name = recipe_dict.FindString("name");
      const auto* image_url = recipe_dict.FindString("image_url");
      const absl::optional<int> viewed_timestamp =
          recipe_dict.FindIntByDottedPath("viewed_timestamp.seconds");
      const auto* site_name = recipe_dict.FindString("site_name");
      const auto* target_url = recipe_dict.FindString("target_url");
      if (!name || !image_url || !target_url) {
        continue;
      }
      auto mojom_recipe = recipes::mojom::Recipe::New();
      mojom_recipe->name = *name;
      mojom_recipe->image_url = GURL(*image_url);
      // GWS timestamps are relative to the Unix Epoch.
      mojom_recipe->info =
          viewed_timestamp ? GetViewedItemText(base::Time::UnixEpoch() +
                                               base::Seconds(*viewed_timestamp))
                           : l10n_util::GetStringUTF8(
                                 IDS_NTP_MODULES_RECIPE_TASKS_RECOMMENDED);
      if (site_name) {
        mojom_recipe->site_name = *site_name;
      }
      mojom_recipe->target_url = GURL(*target_url);
      mojo_recipes.push_back(std::move(mojom_recipe));
    }

    if (related_searches) {
      std::vector<recipes::mojom::RelatedSearchPtr> mojo_related_searches;
      for (const auto& related_search : *related_searches) {
        const base::Value::Dict& related_search_dict = related_search.GetDict();
        auto* text = related_search_dict.FindString("text");
        auto* target_url = related_search_dict.FindString("target_url");
        if (!text || !target_url) {
          continue;
        }
        auto mojo_related_search = recipes::mojom::RelatedSearch::New();
        mojo_related_search->text = *text;
        mojo_related_search->target_url = GURL(*target_url);
        mojo_related_searches.push_back(std::move(mojo_related_search));
      }

      base::UmaHistogramCounts100(
          "NewTabPage.RecipeTasks.RelatedSearchDownloadCount",
          mojo_related_searches.size());
      mojo_task->related_searches = std::move(mojo_related_searches);
    }

    mojo_task->title = *title;
    mojo_task->name = *task_name;
    base::UmaHistogramCounts100("NewTabPage.RecipeTasks.RecipesDownloadCount",
                                mojo_recipes.size());
    mojo_task->recipes = std::move(mojo_recipes);

    std::move(callback).Run(std::move(mojo_task));
    return;
  }
  std::move(callback).Run(nullptr);
}

bool RecipesService::IsTaskDismissed(const std::string& task_name) {
  if (base::FeatureList::IsEnabled(ntp_features::kNtpModulesRedesigned)) {
    return false;
  }
  const base::Value::List& dismissed_tasks =
      profile_->GetPrefs()->GetList(kDismissedTasksPrefName);
  return base::Contains(dismissed_tasks, base::Value(task_name));
}
