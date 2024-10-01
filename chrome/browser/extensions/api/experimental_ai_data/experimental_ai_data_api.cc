// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/extensions/api/experimental_ai_data.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

// Feature to add allow listed extensions remotely.
BASE_FEATURE(kAllowlistedAiDataExtensions,
             "AllowlistedAiDataExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

const base::FeatureParam<std::string> kAllowlistedExtensions{
    &kAllowlistedAiDataExtensions, "allowlisted_extension_ids",
    /*default_value=*/""};

}  // namespace

ExperimentalAiDataGetAiDataFunction::ExperimentalAiDataGetAiDataFunction() =
    default;

ExperimentalAiDataGetAiDataFunction::~ExperimentalAiDataGetAiDataFunction() =
    default;

ExtensionFunction::ResponseAction ExperimentalAiDataGetAiDataFunction::Run() {
  // Check the allowlist and return an error if extension is not allow listed.
  std::string allowlisted_extension_string = kAllowlistedExtensions.Get();
  std::vector<std::string_view> allowlisted_extensions =
      base::SplitStringPiece(allowlisted_extension_string, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (std::find(allowlisted_extensions.begin(), allowlisted_extensions.end(),
                extension_id()) == allowlisted_extensions.end()) {
    return RespondNow(Error("API access restricted for this extension."));
  }

  // In addition to the extension framework channel restriction, we make sure
  // the API is not available on Stable. In particular,
  // extension::switches::kEnableExperimentalExtensionApis allows ignoring those
  // channel restrictions.
  if (chrome::GetChannel() == version_info::Channel::STABLE) {
    return RespondNow(Error("API access restricted to non-Stable channels."));
  }

  auto params = api::experimental_ai_data::GetAiData::Params::Create(args());

  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(), true,
                                    &web_contents)) {
    return RespondNow(Error("Invalid target tab passed in."));
  }

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  if (!ai_data_service) {
    return RespondNow(Error("Incognito profile not supported."));
  }

  const std::string& user_input = params->user_input;
  int dom_node_id = params->dom_node_id;

  ai_data_service->GetAiData(
      dom_node_id, web_contents, user_input,
      base::BindOnce(&ExperimentalAiDataGetAiDataFunction::OnDataCollected,
                     this));

  return RespondLater();
}

void ExperimentalAiDataGetAiDataFunction::OnDataCollected(
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

}  // namespace extensions
