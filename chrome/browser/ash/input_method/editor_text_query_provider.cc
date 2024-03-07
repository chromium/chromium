// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_query_provider.h"

#include <optional>

#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_status.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash::input_method {

namespace {

std::string GetConfigLabelFromFieldTrialConfig() {
  return base::GetFieldTrialParamValue("OrcaEnabled", "config_label");
}

std::unique_ptr<manta::OrcaProvider> CreateProvider(Profile* profile) {
  if (!manta::features::IsMantaServiceEnabled()) {
    return nullptr;
  }

  if (manta::MantaService* service =
          manta::MantaServiceFactory::GetForProfile(profile)) {
    return service->CreateOrcaProvider();
  }

  return nullptr;
}

std::map<std::string, std::string> CreateProviderRequest(
    orca::mojom::TextQueryRequestPtr request) {
  auto& params = request->parameters;
  std::map<std::string, std::string> provider_request;

  InputMethodManager* input_method_manager = InputMethodManager::Get();

  if (input_method_manager != nullptr &&
      input_method_manager->GetActiveIMEState() != nullptr) {
    provider_request["ime"] = extension_ime_util::GetComponentIDByInputMethodID(
        input_method_manager->GetActiveIMEState()
            ->GetCurrentInputMethod()
            .id());
  }

  for (auto it = params.begin(); it != params.end(); ++it) {
    provider_request[it->first] = it->second;
  }

  provider_request["tone"] = request->text_query_id;

  if (auto config_label = GetConfigLabelFromFieldTrialConfig();
      !config_label.empty()) {
    provider_request["config_label"] = config_label;
  }
  return provider_request;
}

orca::mojom::TextQueryErrorCode ConvertErrorCode(
    manta::MantaStatusCode status_code) {
  switch (status_code) {
    case manta::MantaStatusCode::kGenericError:
    case manta::MantaStatusCode::kMalformedResponse:
    case manta::MantaStatusCode::kNoIdentityManager:
      return orca::mojom::TextQueryErrorCode::kUnknown;
    case manta::MantaStatusCode::kInvalidInput:
      return orca::mojom::TextQueryErrorCode::kInvalidArgument;
    case manta::MantaStatusCode::kResourceExhausted:
    case manta::MantaStatusCode::kPerUserQuotaExceeded:
      return orca::mojom::TextQueryErrorCode::kResourceExhausted;
    case manta::MantaStatusCode::kBackendFailure:
      return orca::mojom::TextQueryErrorCode::kBackendFailure;
    case manta::MantaStatusCode::kNoInternetConnection:
      return orca::mojom::TextQueryErrorCode::kNoInternetConnection;
    case manta::MantaStatusCode::kUnsupportedLanguage:
      return orca::mojom::TextQueryErrorCode::kUnsupportedLanguage;
    case manta::MantaStatusCode::kBlockedOutputs:
      return orca::mojom::TextQueryErrorCode::kBlockedOutputs;
    case manta::MantaStatusCode::kRestrictedCountry:
      return orca::mojom::TextQueryErrorCode::kRestrictedRegion;
    case manta::MantaStatusCode::kOk:
      NOTREACHED_NORETURN();
  }
}

orca::mojom::TextQueryErrorPtr ConvertErrorResponse(manta::MantaStatus status) {
  return orca::mojom::TextQueryError::New(ConvertErrorCode(status.status_code),
                                          status.message);
}

std::vector<orca::mojom::TextQueryResultPtr> ParseSuccessResponse(
    const std::string& request_id,
    base::Value::Dict& response) {
  std::vector<orca::mojom::TextQueryResultPtr> results;
  if (auto* output_data_list = response.FindList("outputData")) {
    int result_id = 0;
    for (const auto& data : *output_data_list) {
      if (const auto* text = data.GetDict().FindString("text")) {
        results.push_back(orca::mojom::TextQueryResult::New(
            base::StrCat({request_id, ":", base::NumberToString(result_id)}),
            *text));
        ++result_id;
      }
    }
  }
  return results;
}

std::optional<size_t> GetLengthOfLongestResponse(
    const std::vector<orca::mojom::TextQueryResultPtr>& responses) {
  if (responses.size() == 0) {
    return std::nullopt;
  }
  size_t max_response_length = 0;
  for (const auto& response : responses) {
    if (response->text.length() > max_response_length) {
      max_response_length = response->text.length();
    }
  }
  return max_response_length;
}

}  // namespace

TextQueryProviderForOrca::TextQueryProviderForOrca(
    mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider> receiver,
    Profile* profile,
    EditorMetricsRecorder* metrics_recorder)
    : text_query_provider_receiver_(this, std::move(receiver)),
      orca_provider_(CreateProvider(profile)),
      metrics_recorder_(metrics_recorder) {}

TextQueryProviderForOrca::~TextQueryProviderForOrca() = default;

void TextQueryProviderForOrca::Process(orca::mojom::TextQueryRequestPtr request,
                                       ProcessCallback callback) {
  if (orca_provider_ == nullptr) {
    // TODO: b:300557202 - use the right error code
    auto response = orca::mojom::TextQueryResponse::NewError(
        orca::mojom::TextQueryError::New(
            orca::mojom::TextQueryErrorCode::kInvalidArgument,
            "No orca provider"));
    std::move(callback).Run(std::move(response));
    return;
  }

  metrics_recorder_->LogEditorState(EditorStates::kRequest);

  ++request_id_;
  orca_provider_->Call(
      CreateProviderRequest(std::move(request)),
      base::BindOnce(
          [](const std::string& request_id,
             EditorMetricsRecorder* metrics_recorder,
             ProcessCallback process_callback, base::Value::Dict dict,
             manta::MantaStatus status) {
            if (status.status_code == manta::MantaStatusCode::kOk) {
              auto responses = ParseSuccessResponse(request_id, dict);
              int number_of_responses = responses.size();
              std::optional<size_t> max_response_length =
                  GetLengthOfLongestResponse(responses);

              std::move(process_callback)
                  .Run(orca::mojom::TextQueryResponse::NewResults(
                      std::move(responses)));

              metrics_recorder->LogEditorState(EditorStates::kSuccessResponse);
              metrics_recorder->LogNumberOfResponsesFromServer(
                  number_of_responses);
              if (max_response_length.has_value()) {
                metrics_recorder->LogLengthOfLongestResponseFromServer(
                    *max_response_length);
              }
              return;
            }

            auto error_response = ConvertErrorResponse(status);
            orca::mojom::TextQueryErrorCode error_code = error_response->code;
            std::move(process_callback)
                .Run(orca::mojom::TextQueryResponse::NewError(
                    std::move(error_response)));
            metrics_recorder->LogEditorState(EditorStates::kErrorResponse);
            metrics_recorder->LogEditorState(ToEditorStatesMetric(error_code));
          },
          base::NumberToString(request_id_), metrics_recorder_,
          std::move(callback)));
}

std::optional<mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider>>
TextQueryProviderForOrca::Unbind() {
  if (text_query_provider_receiver_.is_bound()) {
    return text_query_provider_receiver_.Unbind();
  }
  return std::nullopt;
}

}  // namespace ash::input_method
