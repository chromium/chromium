// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manta/orca_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
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
constexpr char kHttpContentType[] = "application/x-protobuf";
constexpr char kEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr base::TimeDelta kTimeoutMs = base::Seconds(90);

const std::unordered_map<std::string, proto::RequestConfig::Tone>&
GetToneMap() {
  static const base::NoDestructor<
      std::unordered_map<std::string, proto::RequestConfig::Tone>>
      tone_map({
          {"UNSPECIFIED", proto::RequestConfig::UNSPECIFIED},
          {"SHORTEN", proto::RequestConfig::SHORTEN},
          {"ELABORATE", proto::RequestConfig::ELABORATE},
          {"REPHRASE", proto::RequestConfig::REPHRASE},
          {"FORMALIZE", proto::RequestConfig::FORMALIZE},
          {"EMOJIFY", proto::RequestConfig::EMOJIFY},
      });

  return *tone_map;
}

absl::optional<proto::Request> ComposeRequest(
    const std::map<std::string, std::string>& input) {
  auto& tone_map = GetToneMap();
  auto tone_iter = input.find("tone");
  if (tone_iter == input.end() ||
      !base::Contains(tone_map, tone_iter->second)) {
    DVLOG(1) << "No valid tone in the parameters";
    return absl::nullopt;
  }

  proto::Request request;
  request.set_feature_name(proto::FeatureName::TEXT_TEST);
  auto& request_config = *request.mutable_request_config();
  request_config.set_tone(tone_map.find(tone_iter->second)->second);

  for (const auto& kv : input) {
    auto* input_data = request.add_input_data();
    input_data->set_tag(kv.first);
    input_data->set_text(kv.second);
  }

  return request;
}

void OnServerResponseOrErrorReceived(
    MantaGenericCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    DCHECK(manta_response == nullptr);
    std::move(callback).Run(base::Value::Dict(), std::move(manta_status));
    return;
  }

  DCHECK(manta_response != nullptr);

  auto output_data_list = base::Value::List();
  for (const auto& output_data : manta_response->output_data()) {
    if (output_data.has_text()) {
      output_data_list.Append(
          base::Value::Dict().Set("text", output_data.text()));
    }
  }

  if (output_data_list.size() == 0) {
    std::move(callback).Run(base::Value::Dict(),
                            {MantaStatusCode::kBlockedOutputs, std::string()});
    return;
  }

  std::move(callback).Run(
      base::Value::Dict().Set("outputData", std::move(output_data_list)),
      std::move(manta_status));
}

}  // namespace

OrcaProvider::OrcaProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

OrcaProvider::~OrcaProvider() = default;

void OrcaProvider::Call(const std::map<std::string, std::string>& input,
                        MantaGenericCallback done_callback) {
  absl::optional<proto::Request> request = ComposeRequest(input);
  if (request == absl::nullopt) {
    std::move(done_callback)
        .Run(base::Value::Dict(),
             {MantaStatusCode::kInvalidInput, std::string()});
    return;
  }

  std::string serialized_request;
  request.value().SerializeToString(&serialized_request);

  std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcher(
      GURL{kEndpointUrl}, {kOAuthScope}, serialized_request);

  EndpointFetcher* const fetcher_ptr = fetcher.get();
  MantaInternalCallback internal_callback = base::BindOnce(
      &OnServerResponseOrErrorReceived, std::move(done_callback));
  fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                    std::move(internal_callback),
                                    std::move(fetcher)));
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
