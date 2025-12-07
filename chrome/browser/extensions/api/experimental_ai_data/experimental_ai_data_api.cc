// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/strings/string_split.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/experimental_ai_data.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

ExperimentalAiDataApiFunction::ExperimentalAiDataApiFunction() = default;

ExperimentalAiDataApiFunction::~ExperimentalAiDataApiFunction() = default;

bool ExperimentalAiDataApiFunction::PreRunValidation(std::string* error) {
  // Check the allowlist and return an error if extension is not allow listed.
  if (!AiDataKeyedService::IsExtensionAllowlistedForData(extension_id())) {
    *error = "API access restricted for this extension.";
    return false;
  }

  if (GetCurrentChannel() == version_info::Channel::STABLE &&
      !AiDataKeyedService::IsExtensionAllowlistedForStable(extension_id())) {
    *error = "API access not allowed on this channel.";
    return false;
  }

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  if (!ai_data_service) {
    *error = "Incognito profile not supported.";
    return false;
  }
  DCHECK(ai_data_service);

  return true;
}

void ExperimentalAiDataApiFunction::OnDataCollected(
    AiDataKeyedService::AiData browser_collected_data) {
  if (!browser_collected_data) {
    return Respond(
        Error("Data collection failed likely due to browser state change."));
  }
  // Convert Proto to bytes to send over the API channel.
  const size_t size = browser_collected_data->ByteSizeLong();
  std::vector<uint8_t> data_buffer(size);

  browser_collected_data->SerializeToArray(&data_buffer[0], size);
  Respond(ArgumentList(api::experimental_ai_data::GetAiData::Results::Create(
      std::move(data_buffer))));
}

ExperimentalAiDataGetAiDataFunction::ExperimentalAiDataGetAiDataFunction() =
    default;

ExperimentalAiDataGetAiDataFunction::~ExperimentalAiDataGetAiDataFunction() =
    default;

ExtensionFunction::ResponseAction ExperimentalAiDataGetAiDataFunction::Run() {
  auto params = api::experimental_ai_data::GetAiData::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(), true,
                                    &web_contents)) {
    return RespondNow(Error("Invalid target tab passed in."));
  }
  DCHECK(web_contents);

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  DCHECK(ai_data_service);

  ai_data_service->GetAiData(
      params->dom_node_id, web_contents, params->user_input,
      base::BindOnce(&ExperimentalAiDataGetAiDataFunction::OnDataCollected,
                     this));
  return RespondLater();
}

ExperimentalAiDataGetAiDataWithSpecifierFunction::
    ExperimentalAiDataGetAiDataWithSpecifierFunction() = default;

ExperimentalAiDataGetAiDataWithSpecifierFunction::
    ~ExperimentalAiDataGetAiDataWithSpecifierFunction() = default;

ExtensionFunction::ResponseAction
ExperimentalAiDataGetAiDataWithSpecifierFunction::Run() {
  auto params =
      api::experimental_ai_data::GetAiDataWithSpecifier::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(), true,
                                    &web_contents)) {
    return RespondNow(Error("Invalid target tab passed in."));
  }
  DCHECK(web_contents);

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  DCHECK(ai_data_service);

  // De-serailizing protos is safe per
  // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md
  optimization_guide::proto::ModelPrototypingCollectionSpecifier specifier;
  if (!specifier.ParseFromArray(params->ai_data_specifier.data(),
                                params->ai_data_specifier.size())) {
    return RespondNow(Error("Parsing ai data specifier failed."));
  }

  ai_data_service->GetAiDataWithSpecifier(
      web_contents, specifier,
      base::BindOnce(
          &ExperimentalAiDataGetAiDataWithSpecifierFunction::OnDataCollected,
          this));
  return RespondLater();
}

}  // namespace extensions
