// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/manatee/manatee_cache.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace app_list {
namespace {

// Maximum accepted size of an ItemSuggest response. 4MB.
constexpr int kMaxResponseSizeBytes = 2048 * 2048;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("launcher_manatee", R"(
      semantics {
        sender: "Launcher suggested manatee details"
        description:
          "Query to be sent to Manatee."
        trigger:
          "Upon a query being entered into launcher search."
        data:
          "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "negeend@google.com"
          }
          contacts {
            email: "laurencom@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        last_reviewed: "2024-01-02"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This cannot be disabled except by policy."
        chrome_policy {
          DriveDisabled {
            DriveDisabled: true
          }
        }
      })");

std::optional<EmbeddingsList> GetList(const base::Value* value) {
  if (!value->is_dict()) {
    return std::nullopt;
  }

  const auto* field = value->GetDict().FindList("embedding");
  if (!field) {
    return std::nullopt;
  }

  EmbeddingsList embeddings;
  for (const auto& embedding_list : *field) {
    std::vector<double> embedding_vec;
    for (const auto& embedding_val : embedding_list.GetList()) {
      embedding_vec.push_back(embedding_val.GetDouble());
    }
    embeddings.push_back(embedding_vec);
  }

  return embeddings;
}

}  // namespace

ManateeCache::ManateeCache(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : profile_(profile), url_loader_factory_(url_loader_factory) {}

ManateeCache::~ManateeCache() = default;

void ManateeCache::RegisterCallback(ManateeCache::OnResultsCallback callback) {
  results_callback_ = std::move(callback);
}

std::string ManateeCache::GetRequestBody(std::string message) {
  static constexpr char kRequestBody[] = R"({
        "text": $1
      })";
  return base::ReplaceStringPlaceholders(kRequestBody, {message}, nullptr);
}

std::string ManateeCache::VectorToString(std::vector<std::string> messages) {
  std::string result = "[";

  for (size_t i = 0; i < messages.size(); ++i) {
    result += "\"" + messages[i] + "\"";
    if (i != messages.size() - 1) {
      result += ", ";
    }
  }
  result += "]";
  return result;
}

void ManateeCache::UrlLoader(std::vector<std::string> messages) {
  weak_factory_.InvalidateWeakPtrs();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make a new request. This destroys any existing |url_loader_| which will
  // cancel that request if it is in-progress.

  url_loader_ = MakeRequestLoader();
  url_loader_->SetRetryOptions(5, network::SimpleURLLoader::RETRY_ON_5XX);

  url_loader_->AttachStringForUpload(GetRequestBody(VectorToString(messages)),
                                     "application/json");
  // Perform the request.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ManateeCache::OnJsonReceived, weak_factory_.GetWeakPtr()),
      kMaxResponseSizeBytes);
}

void ManateeCache::OnJsonReceived(
    const std::unique_ptr<std::string> json_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  if (net_error != net::OK) {
    return;
  } else if (!json_response || json_response->empty()) {
    return;
  }

  // Parse the JSON response from ItemSuggest.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *json_response,
      base::BindOnce(&ManateeCache::OnJsonParsed, weak_factory_.GetWeakPtr()));
}

void ManateeCache::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    return;
  }

  std::optional<EmbeddingsList> embeddings = GetList(&*result).value();
  if (!embeddings.has_value() || results_callback_.is_null()) {
    return;
  }
  std::move(results_callback_).Run(embeddings.value());
  results_callback_.Reset();
  response_ = std::move(embeddings.value());

}

EmbeddingsList ManateeCache::GetResponse() {
  return response_;
}

std::unique_ptr<network::SimpleURLLoader> ManateeCache::MakeRequestLoader() {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->method = "POST";
  resource_request->url = server_url_;
  // Do not allow cookies.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Ignore the cache because we always want fresh results.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  DCHECK(resource_request->url.is_valid());
  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kTrafficAnnotation);
}
}  // namespace app_list
