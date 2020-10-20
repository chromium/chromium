// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/shopping_tasks/shopping_tasks_service.h"

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
const char kNewTabShoppingTasksApiPath[] = "/async/newtab_shopping_tasks";
const char kXSSIResponsePreamble[] = ")]}'";
const char kDismissedTasksPrefName[] = "NewTabPage.DismissedShoppingTasks";

GURL GetApiUrl(const std::string& application_locale) {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }
  return net::AppendQueryParameter(
      google_base_url.Resolve(kNewTabShoppingTasksApiPath), "hl",
      application_locale);
}
}  // namespace

ShoppingTasksService::ShoppingTasksService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const std::string& application_locale)
    : profile_(profile),
      url_loader_factory_(url_loader_factory),
      application_locale_(application_locale) {}

ShoppingTasksService::~ShoppingTasksService() = default;

// static
void ShoppingTasksService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedTasksPrefName);
}

void ShoppingTasksService::Shutdown() {}

void ShoppingTasksService::GetPrimaryShoppingTask(
    ShoppingTaskCallback callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("shopping_tasks_service", R"(
        semantics {
          sender: "Shopping Tasks Service"
          description: "This service downloads shopping tasks, which is "
            "information related to the user's currently active shopping "
            "search journeys such as visisted and recommended products. "
            "Shopping tasks will be displayed on the new tab page to help the "
            "user to continue their search journey. Shopping tasks are queried "
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
      base::BindOnce(&ShoppingTasksService::OnDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get(),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void ShoppingTasksService::DismissShoppingTask(const std::string& task_name) {
  ListPrefUpdate update(profile_->GetPrefs(), kDismissedTasksPrefName);
  update->AppendIfNotPresent(std::make_unique<base::Value>(task_name));
}

void ShoppingTasksService::RestoreShoppingTask(const std::string& task_name) {
  ListPrefUpdate update(profile_->GetPrefs(), kDismissedTasksPrefName);
  update->EraseListValue(base::Value(task_name));
}

void ShoppingTasksService::OnDataLoaded(network::SimpleURLLoader* loader,
                                        ShoppingTaskCallback callback,
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
      base::BindOnce(&ShoppingTasksService::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingTasksService::OnJsonParsed(
    ShoppingTaskCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    std::move(callback).Run(nullptr);
    return;
  }
  // We receive a list of shopping tasks ordered from highest to lowest
  // priority. We only support showing a single task though. Therefore, pick the
  // first task.
  auto* shopping_tasks = result.value->FindListPath("update.shopping_tasks");
  if (!shopping_tasks || shopping_tasks->GetList().size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }
  for (const auto& shopping_task : shopping_tasks->GetList()) {
    auto* title = shopping_task.FindStringPath("title");
    auto* task_name = shopping_task.FindStringPath("task_name");
    auto* products = shopping_task.FindListPath("products");
    auto* related_searches = shopping_task.FindListPath("related_searches");
    if (!title || !task_name || !products || !related_searches ||
        products->GetList().size() == 0) {
      continue;
    }
    if (IsShoppingTaskDismissed(*task_name)) {
      continue;
    }
    std::vector<shopping_tasks::mojom::ProductPtr> mojo_products;
    for (const auto& product : products->GetList()) {
      auto* name = product.FindStringPath("name");
      auto* image_url = product.FindStringPath("image_url");
      auto* price = product.FindStringPath("price");
      auto* info = product.FindStringPath("info");
      auto* target_url = product.FindStringPath("target_url");
      if (!name || !image_url || !price || !info || !target_url) {
        continue;
      }
      auto mojo_product = shopping_tasks::mojom::Product::New();
      mojo_product->name = *name;
      mojo_product->image_url = GURL(*image_url);
      mojo_product->price = *price;
      mojo_product->info = *info;
      mojo_product->target_url = GURL(*target_url);
      mojo_products.push_back(std::move(mojo_product));
    }
    std::vector<shopping_tasks::mojom::RelatedSearchPtr> mojo_related_searches;
    for (const auto& related_search : related_searches->GetList()) {
      auto* text = related_search.FindStringPath("text");
      auto* target_url = related_search.FindStringPath("target_url");
      if (!text || !target_url) {
        continue;
      }
      auto mojo_related_search = shopping_tasks::mojom::RelatedSearch::New();
      mojo_related_search->text = *text;
      mojo_related_search->target_url = GURL(*target_url);
      mojo_related_searches.push_back(std::move(mojo_related_search));
    }
    auto mojo_shopping_task = shopping_tasks::mojom::ShoppingTask::New();
    mojo_shopping_task->title = *title;
    mojo_shopping_task->name = *task_name;
    mojo_shopping_task->products = std::move(mojo_products);
    mojo_shopping_task->related_searches = std::move(mojo_related_searches);
    std::move(callback).Run(std::move(mojo_shopping_task));
    return;
  }
  std::move(callback).Run(nullptr);
}

bool ShoppingTasksService::IsShoppingTaskDismissed(
    const std::string& task_name) {
  const base::ListValue* dismissed_tasks =
      profile_->GetPrefs()->GetList(kDismissedTasksPrefName);
  DCHECK(dismissed_tasks);
  return dismissed_tasks->Find(base::Value(task_name)) !=
         dismissed_tasks->end();
}
