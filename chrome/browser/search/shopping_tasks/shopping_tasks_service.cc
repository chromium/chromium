// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/shopping_tasks/shopping_tasks_service.h"

#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {
const char kNewTabShoppingTasksApiPath[] = "/async/newtab_shopping_tasks";
const char kXSSIResponsePreamble[] = ")]}'";

GURL GetApiUrl() {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }
  return google_base_url.Resolve(kNewTabShoppingTasksApiPath);
}
}  // namespace

ShoppingTasksService::ShoppingTasksService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile)
    : url_loader_factory_(url_loader_factory) {}

ShoppingTasksService::~ShoppingTasksService() = default;

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
  resource_request->url = GetApiUrl();
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));

  loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation));
  loaders_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ShoppingTasksService::OnDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get(),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void ShoppingTasksService::OnDataLoaded(network::SimpleURLLoader* loader,
                                        ShoppingTaskCallback callback,
                                        std::unique_ptr<std::string> response) {
  auto net_error = loader->NetError();
  base::EraseIf(loaders_, [loader](const auto& target) {
    return loader == target.get();
  });

  if (net_error != net::OK || !response) {
    std::move(callback).Run(base::nullopt);
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
    std::move(callback).Run(base::nullopt);
    return;
  }
  // We receive a list of shopping tasks ordered from highest to lowest
  // priority. We only support showing a single task though. Therefore, pick the
  // first task.
  auto* shopping_tasks = result.value->FindListPath("update.shopping_tasks");
  if (!shopping_tasks || shopping_tasks->GetList().size() == 0) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  auto* title = shopping_tasks->GetList()[0].FindStringPath("title");
  auto* products = shopping_tasks->GetList()[0].FindListPath("products");
  auto* related_searches =
      shopping_tasks->GetList()[0].FindListPath("related_searches");
  if (!title || !products || !related_searches) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::vector<ShoppingTasksData::Product> products_list;
  for (const auto& product : products->GetList()) {
    auto* name = product.FindStringPath("name");
    auto* image_url = product.FindStringPath("image_url");
    auto* price = product.FindStringPath("price");
    auto* info = product.FindStringPath("info");
    auto* target_url = product.FindStringPath("target_url");
    if (!name || !image_url || !price || !info || !target_url) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    ShoppingTasksData::Product product_struct;
    product_struct.name = *name;
    product_struct.image_url = GURL(*image_url);
    product_struct.price = *price;
    product_struct.info = *info;
    product_struct.target_url = GURL(*target_url);
    products_list.push_back(product_struct);
  }
  std::vector<ShoppingTasksData::RelatedSearch> related_searches_list;
  for (const auto& related_search : related_searches->GetList()) {
    auto* text = related_search.FindStringPath("text");
    auto* target_url = related_search.FindStringPath("target_url");
    if (!text || !target_url) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    ShoppingTasksData::RelatedSearch related_search_struct;
    related_search_struct.text = *text;
    related_search_struct.target_url = GURL(*target_url);
    related_searches_list.push_back(related_search_struct);
  }
  ShoppingTasksData data;
  data.title = *title;
  data.products = products_list;
  data.related_searches = related_searches_list;
  std::move(callback).Run(data);
}
