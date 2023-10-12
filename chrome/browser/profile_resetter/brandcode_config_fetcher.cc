// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const int kDownloadTimeoutSec = 10;
const char kPostXml[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<request"
    "    version=\"chromeprofilereset-1.1\""
    "    protocol=\"3.0\""
    "    installsource=\"profilereset\">"
    "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\">"
    "    <data name=\"install\" index=\"__BRANDCODE_PLACEHOLDER__\"/>"
    "  </app>"
    "</request>";

// Returns the query to the server which can be used to retrieve the config.
// |brand| is a brand code, it mustn't be empty.
std::string GetUploadData(const std::string& brand) {
  DCHECK(!brand.empty());
  std::string data(kPostXml);
  const std::string placeholder("__BRANDCODE_PLACEHOLDER__");
  size_t placeholder_pos = data.find(placeholder);
  DCHECK(placeholder_pos != std::string::npos);
  data.replace(placeholder_pos, placeholder.size(), brand);
  return data;
}

}  // namespace

BrandcodeConfigFetcher::BrandcodeConfigFetcher(
    network::mojom::URLLoaderFactory* url_loader_factory,
    FetchCallback callback,
    const GURL& url,
    const std::string& brandcode)
    : fetch_callback_(std::move(callback)), weak_ptr_factory_(this) {
  DCHECK(!brandcode.empty());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("brandcode_config", R"(
        semantics {
          sender: "Brandcode Configuration Fetcher"
          description:
            "Chrome installation can be non-organic. That means that Chrome "
            "is distributed by partners and it has a brand code associated "
            "with that partner. For the settings reset operation, Chrome needs "
            "to know the default settings which are partner specific."
          trigger: "'Reset Settings' invocation from Chrome settings."
          data: "Brandcode."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled and is only invoked by user "
            "request."
          policy_exception_justification:
            "Not implemented, considered not useful as enterprises don't need "
            "to install Chrome in a non-organic fashion."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Accept", "text/xml");
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(GetUploadData(brandcode),
                                            "text/xml");
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&BrandcodeConfigFetcher::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  // Abort the download attempt if it takes too long.
  download_timer_.Start(FROM_HERE, base::Seconds(kDownloadTimeoutSec), this,
                        &BrandcodeConfigFetcher::OnDownloadTimeout);
}

BrandcodeConfigFetcher::~BrandcodeConfigFetcher() {}

void BrandcodeConfigFetcher::SetCallback(FetchCallback callback) {
  fetch_callback_ = std::move(callback);
}

void BrandcodeConfigFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  const bool is_valid_response =
      response_body && simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->mime_type == "text/xml";

  // Release resources before potentially running the callback.
  simple_url_loader_.reset();
  download_timer_.Stop();

  if (is_valid_response) {
    data_decoder::DataDecoder::ParseXmlIsolated(
        *response_body,
        data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
        base::BindOnce(&BrandcodeConfigFetcher::OnXmlConfigParsed,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    std::move(fetch_callback_).Run();
  }

  // `this` may now be deleted from `fetch_callback_`.
}

void BrandcodeConfigFetcher::OnXmlConfigParsed(
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  // The |fetch_callback_| is called in the case of either success or
  // failure. The difference is whether |default_settings_| is populated.
  base::ScopedClosureRunner scoped_closure(std::move(fetch_callback_));

  if (!value_or_error.has_value())
    return;

  const base::Value* node = &*value_or_error;
  if (!data_decoder::IsXmlElementNamed(*node, "response"))
    return;

  // Descend this tag path to find the node containing the JSON data.
  const char* kDataPath[] = {"app", "data"};
  for (const auto* tag : kDataPath) {
    node = data_decoder::GetXmlElementChildWithTag(*node, tag);
    if (!node)
      return;
  }

  // Extract the text JSON data from the "data" node to specify the new
  // settings.
  std::string master_prefs;
  if (node && data_decoder::GetXmlElementText(*node, &master_prefs)) {
    default_settings_ =
        std::make_unique<BrandcodedDefaultSettings>(master_prefs);
  }
}

void BrandcodeConfigFetcher::OnDownloadTimeout() {
  if (simple_url_loader_) {
    simple_url_loader_.reset();
    std::move(fetch_callback_).Run();
  }
}
