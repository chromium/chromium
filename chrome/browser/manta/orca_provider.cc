// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/orca_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_orca";
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/json; charset=UTF-8";
constexpr char kEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr base::TimeDelta kTimeoutMs = base::Seconds(90);

void OnJsonParsed(OrcaProviderCallback callback,
                  data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_dict()) {
    DVLOG(1) << "Failed to parse server response";
    std::move(callback).Run(
        base::Value::Dict()
            .Set("error", true)
            .Set("error_message", "False to parse server response"));
    return;
  }

  std::move(callback).Run(std::move(*result).TakeDict());
}

void OnEndpointFetcherComplete(OrcaProviderCallback callback,
                               std::unique_ptr<EndpointFetcher> fetcher,
                               std::unique_ptr<EndpointResponse> responses) {
  if (responses->error_type.has_value() ||
      responses->http_status_code != net::HTTP_OK) {
    auto error_dict =
        base::Value::Dict()
            .Set("error", true)
            .Set("error_message", "Unexpected response from server")
            .Set("http_status_code",
                 static_cast<int32_t>(responses->http_status_code))
            .Set("response", responses->response);

    if (responses->error_type.has_value()) {
      error_dict.Set("error_type",
                     static_cast<int32_t>(responses->error_type.value()));
    }

    std::move(callback).Run(std::move(error_dict));
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response, base::BindOnce(&OnJsonParsed, std::move(callback)));
}

absl::optional<std::string> ComposeRequestJson(
    const std::map<std::string, std::string>& input) {
  auto tone_iter = input.find("tone");
  if (tone_iter == input.end()) {
    DVLOG(1) << "Failed to find a tone in the parameters";
    return absl::nullopt;
  }

  base::Value::Dict request_dict =
      base::Value::Dict()
          .Set("feature_name", "CHROMEOS_COPY_EDITOR_WRITER")
          .Set("client_info", base::Value::Dict().Set("client_type", "UNKNOWN"))
          .Set("model_config",
               base::Value::Dict().Set("tone", tone_iter->second));

  base::Value::List input_data_list;
  for (const auto& kv : input) {
    input_data_list.Append(
        base::Value::Dict().Set("tag", kv.first).Set("text", kv.second));
  }

  request_dict.Set("input_data", std::move(input_data_list));

  return base::WriteJson(request_dict);
}

}  // namespace

OrcaProvider::OrcaProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

OrcaProvider::~OrcaProvider() = default;

void OrcaProvider::Call(const std::map<std::string, std::string>& input,
                        OrcaProviderCallback done_callback) {
  absl::optional<std::string> request_json = ComposeRequestJson(input);
  if (request_json == absl::nullopt) {
    std::move(done_callback)
        .Run(base::Value::Dict()
                 .Set("error", true)
                 .Set("error_message", "Fail to prepare request"));
    return;
  }

  std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcher(
      GURL{kEndpointUrl}, {kOAuthScope}, request_json.value());

  EndpointFetcher* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                    std::move(done_callback),
                                    std::move(fetcher)));
}

void OrcaProvider::HandleResponse(
    EndpointFetcherCallback done_callback,
    std::unique_ptr<EndpointFetcher> /* endpoint_fetcher */,
    std::unique_ptr<EndpointResponse> response) {
  std::move(done_callback).Run(std::move(response));
}

std::unique_ptr<EndpointFetcher> OrcaProvider::CreateEndpointFetcher(
    const GURL& url,
    const std::vector<std::string>& scopes,
    const std::string& post_data) {
  // TODO(b:288019728): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/kOauthConsumerName,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*scopes=*/scopes,
      /*timeout_ms=*/kTimeoutMs.InMilliseconds(),
      /*post_data=*/post_data,
      /*annotation_tag=*/MISSING_TRAFFIC_ANNOTATION,
      /*identity_manager=*/identity_manager_,
      /*consent_level=*/signin::ConsentLevel::kSignin);
}

}  // namespace manta
