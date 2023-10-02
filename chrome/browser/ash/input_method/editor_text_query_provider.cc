// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_query_provider.h"

#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/manta/manta_service.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/manta/manta_status.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "components/manta/features.h"

namespace ash::input_method {

namespace {
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

  for (auto it = params.begin(); it != params.end(); ++it) {
    provider_request[it->first] = it->second;
  }

  provider_request["tone"] = request->text_query_id;
  return provider_request;
}

orca::mojom::TextQueryErrorCode ConvertErrorCode(
    manta::MantaStatusCode status_code) {
  switch (status_code) {
    case manta::MantaStatusCode::kGenericError:
    case manta::MantaStatusCode::kMalformedResponse:
      return orca::mojom::TextQueryErrorCode::kUnknown;
    case manta::MantaStatusCode::kInvalidInput:
      return orca::mojom::TextQueryErrorCode::kInvalidArgument;
    case manta::MantaStatusCode::kResourceExhausted:
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
    base::Value::Dict& response) {
  std::vector<orca::mojom::TextQueryResultPtr> results;
  if (auto* output_data_list = response.FindList("outputData")) {
    int result_id = 0;
    for (const auto& data : *output_data_list) {
      if (const auto* text = data.GetDict().FindString("text")) {
        results.push_back(orca::mojom::TextQueryResult::New(
            base::NumberToString(result_id), *text));
        ++result_id;
      }
    }
  }
  return results;
}

}  // namespace

EditorTextQueryProvider::EditorTextQueryProvider(
    mojo::PendingAssociatedReceiver<orca::mojom::TextQueryProvider> receiver,
    Profile* profile)
    : text_query_provider_receiver_(this, std::move(receiver)),
      orca_provider_(CreateProvider(profile)) {}

EditorTextQueryProvider::~EditorTextQueryProvider() = default;

void EditorTextQueryProvider::OnProfileChanged(Profile* profile) {
  orca_provider_ = CreateProvider(profile);
}

void EditorTextQueryProvider::Process(orca::mojom::TextQueryRequestPtr request,
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
  orca_provider_->Call(
      CreateProviderRequest(std::move(request)),
      base::BindOnce(
          [](ProcessCallback process_callback, base::Value::Dict dict,
             manta::MantaStatus status) {
            std::move(process_callback)
                .Run(status.status_code == manta::MantaStatusCode::kOk
                         ? orca::mojom::TextQueryResponse::NewResults(
                               ParseSuccessResponse(dict))
                         : orca::mojom::TextQueryResponse::NewError(
                               ConvertErrorResponse(status)));
          },
          std::move(callback)));
}

}  // namespace ash::input_method
