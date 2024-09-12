// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "chrome/common/extensions/api/experimental_ai_data.h"

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

  std::vector<uint8_t> data_buffer;
  return RespondNow(
      ArgumentList(api::experimental_ai_data::GetAiData::Results::Create(
          std::move(data_buffer))));
}

}  // namespace extensions
