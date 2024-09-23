// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_add_feature_flags.h"

#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace base {

ScopedAddFeatureFlags::ScopedAddFeatureFlags(CommandLine* command_line)
    : command_line_(command_line) {
  std::string enabled_features =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled_features =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);
  for (std::string_view feature :
       FeatureList::SplitFeatureListString(enabled_features)) {
    enabled_features_.emplace_back(feature);
  }
  for (std::string_view feature :
       FeatureList::SplitFeatureListString(disabled_features)) {
    disabled_features_.emplace_back(feature);
  }
}

ScopedAddFeatureFlags::~ScopedAddFeatureFlags() {
  command_line_->RemoveSwitch(switches::kEnableFeatures);
  command_line_->AppendSwitchASCII(switches::kEnableFeatures,
                                   JoinString(enabled_features_, ","));
  command_line_->RemoveSwitch(switches::kDisableFeatures);
  command_line_->AppendSwitchASCII(switches::kDisableFeatures,
                                   JoinString(disabled_features_, ","));
}

void ScopedAddFeatureFlags::EnableIfNotSet(const Feature& feature) {
  AddFeatureIfNotSet(feature, /*suffix=*/"", /*enable=*/true);
}

void ScopedAddFeatureFlags::EnableIfNotSetWithParameter(
    const Feature& feature,
    std::string_view name,
    std::string_view value) {
  std::string suffix = StrCat({":", name, "/", value});
  AddFeatureIfNotSet(feature, suffix, true /* enable */);
}

void ScopedAddFeatureFlags::DisableIfNotSet(const Feature& feature) {
  AddFeatureIfNotSet(feature, /*suffix=*/"", /*enable=*/false);
}

bool ScopedAddFeatureFlags::IsEnabled(const Feature& feature) {
  return IsEnabledWithParameter(feature, /*parameter_name=*/"",
                                /*parameter_value=*/"");
}

bool ScopedAddFeatureFlags::IsEnabledWithParameter(
    const Feature& feature,
    std::string_view parameter_name,
    std::string_view parameter_value) {
  std::string feature_name = feature.name;
  if (!parameter_name.empty()) {
    StrAppend(&feature_name, {":", parameter_name, "/", parameter_value});
  }
  if (Contains(disabled_features_, feature_name))
    return false;
  if (Contains(enabled_features_, feature_name))
    return true;
  return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
}

void ScopedAddFeatureFlags::AddFeatureIfNotSet(const Feature& feature,
                                               std::string_view suffix,
                                               bool enable) {
  std::string feature_name = StrCat({feature.name, suffix});
  if (Contains(enabled_features_, feature_name) ||
      Contains(disabled_features_, feature_name)) {
    return;
  }
  if (enable) {
    enabled_features_.emplace_back(feature_name);
  } else {
    disabled_features_.emplace_back(feature_name);
  }
}

}  // namespace base
