// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_query_provider.h"

#include "base/values.h"
#include "chrome/browser/manta/manta_service.h"
#include "chrome/browser/manta/manta_service_factory.h"

namespace ash::input_method {

namespace {
std::unique_ptr<manta::OrcaProvider> CreateProvider(Profile* profile) {
  if (manta::MantaServiceFactory::GetInstance() == nullptr) {
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
  return provider_request;
}

orca::mojom::TextQueryResponsePtr ParseResponse(
    base::Value::Dict& raw_response) {
  if (raw_response.FindBool("error").value_or(false)) {
    // TODO: b:300557202 - use the right error code
    auto * error_message = raw_response.FindString("error_message");
    return orca::mojom::TextQueryResponse::NewError(
        orca::mojom::TextQueryError::New(
            orca::mojom::TextQueryErrorCode::kInvalidArgument,
            error_message != nullptr ? *error_message : ""));
  }

  std::vector<orca::mojom::TextQueryResultPtr> results;
  if (auto* output_data_list = raw_response.FindList("outputData")) {
    for (const auto& data : *output_data_list) {
      if (const auto* text = data.GetDict().FindString("text")) {
        results.push_back(orca::mojom::TextQueryResult::New("", *text));
      }
    }
  }
  return orca::mojom::TextQueryResponse::NewResults(std::move(results));
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
          [](ProcessCallback process_callback, base::Value::Dict dict) {
            auto response = ParseResponse(dict);
            std::move(process_callback).Run(std::move(response));
          },
          std::move(callback)));
}

}  // namespace ash::input_method
