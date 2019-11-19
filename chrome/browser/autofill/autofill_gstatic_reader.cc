// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_gstatic_reader.h"

#include <list>
#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {

namespace {
static const char kTokenizationBinRangeWhitelistKey[] =
    "cpan_eligible_bin_wl_regex";
static const char kTokenizationMerchantWhitelistKey[] =
    "cpan_eligible_merchant_wl";
static const char kTokenizationBinRangeWhitelistURL[] =
    "https://www.gstatic.com/autofill/hourly/bins.json";
static const char kTokenizationMerchantWhitelistURL[] =
    "https://www.gstatic.com/autofill/weekly/merchants.json";
static const size_t kMaxDownloadSize = 30 * 1024;
}  // namespace

AutofillGstaticReader::AutofillGstaticReader() {}

AutofillGstaticReader::~AutofillGstaticReader() {}

void AutofillGstaticReader::SetUp() {
  if (!setup_called_) {
    setup_called_ = true;
    LoadDataAsList(GURL(kTokenizationBinRangeWhitelistURL),
                   kTokenizationBinRangeWhitelistKey);
    LoadDataAsList(GURL(kTokenizationMerchantWhitelistURL),
                   kTokenizationMerchantWhitelistKey);
  }
}

AutofillGstaticReader* AutofillGstaticReader::GetInstance() {
  static base::NoDestructor<AutofillGstaticReader> instance;
  return instance.get();
}

std::vector<std::string>
AutofillGstaticReader::GetTokenizationMerchantWhitelist() const {
  DCHECK(setup_called_);  // Ensure data has been loaded.
  return tokenization_merchant_whitelist_;
}

std::vector<std::string>
AutofillGstaticReader::GetTokenizationBinRangesWhitelist() const {
  DCHECK(setup_called_);  // Ensure data has been loaded.
  return tokenization_bin_range_whitelist_;
}

void AutofillGstaticReader::LoadDataAsList(const GURL& url,
                                           const std::string& key) {
  DCHECK(setup_called_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->url = url;
  resource_request->method = "GET";
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("load_autofill_gstatic_data", R"(
        semantics {
          sender: "Autofill"
          description:
            "Downloads data used to decide when to offer Autofill features, "
            "such as whitelists of eligible websites."
          trigger: "Triggered once on Chrome startup."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can disable this feature by disabling the Payments and "
            "Addresses settings in Chrome's settings under Autofill. This "
            "feature is always enabled unless both Payments AND Addresses "
            "Autofill are disabled."
          chrome_policy {
            AutofillCreditCardEnabled {
                policy_options {mode: MANDATORY}
                AutofillCreditCardEnabled: true
            }
          }
          chrome_policy {
            AutofillAddressEnabled {
                policy_options {mode: MANDATORY}
                AutofillAddressEnabled: true
            }
          }
        }
        comments: "Both the AutofillAddressEnabled and "
          "AutofillCreditCardEnabled policies needs to be disabled for this "
          "network request to be disabled.")");
  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_loader->SetAllowHttpErrorResults(true);

  // Transfer ownership of the loader into |url_loaders_|. Temporarily hang
  // onto the raw pointer to kick off the request;
  // transferring ownership (std::move) invalidates the |simple_loader|
  // variable.
  auto* raw_simple_loader = simple_loader.get();
  url_loaders_.push_back(std::move(simple_loader));
  if (g_browser_process->system_network_context_manager()->HasInstance()) {
    raw_simple_loader->DownloadToString(
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory()
            .get(),
        base::BindOnce(&AutofillGstaticReader::OnSimpleLoaderComplete,
                       base::Unretained(this), --url_loaders_.end(), key),
        kMaxDownloadSize);
  }
}

void AutofillGstaticReader::OnSimpleLoaderComplete(
    std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    const std::string& key,
    std::unique_ptr<std::string> response_body) {
  // Move the loader out of the active loaders list.
  std::unique_ptr<network::SimpleURLLoader> simple_loader = std::move(*it);
  url_loaders_.erase(it);
  SetListClassVariable(ParseListJSON(std::move(response_body), key), key);
}

// static
std::vector<std::string> AutofillGstaticReader::ParseListJSON(
    std::unique_ptr<std::string> response_body,
    const std::string& key) {
  if (!response_body)
    return {};
  base::Optional<base::Value> data = base::JSONReader::Read(*response_body);
  if (data == base::nullopt || !data->is_dict())
    return {};
  base::Value* raw_result = data->FindKey(key);
  if (!raw_result || !raw_result->is_list())
    return {};
  std::vector<std::string> result;
  for (const base::Value& value : raw_result->GetList()) {
    if (value.is_string())
      result.push_back(value.GetString());
  }
  return result;
}

void AutofillGstaticReader::SetListClassVariable(
    std::vector<std::string> result,
    const std::string& key) {
  if (key == kTokenizationBinRangeWhitelistKey) {
    tokenization_bin_range_whitelist_ = result;
  } else if (key == kTokenizationMerchantWhitelistKey) {
    tokenization_merchant_whitelist_ = result;
  }
}

}  // namespace autofill
