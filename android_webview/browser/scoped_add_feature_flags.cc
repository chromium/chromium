// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/scoped_add_feature_flags.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"

namespace android_webview {

ScopedAddFeatureFlags::ScopedAddFeatureFlags(base::CommandLine* cl) : cl_(cl) {
  std::string enabled_features =
      cl->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled_features =
      cl->GetSwitchValueASCII(switches::kDisableFeatures);
  for (auto& sp : base::FeatureList::SplitFeatureListString(enabled_features))
    enabled_features_.emplace_back(sp);
  for (auto& sp : base::FeatureList::SplitFeatureListString(disabled_features))
    disabled_features_.emplace_back(sp);
}

ScopedAddFeatureFlags::~ScopedAddFeatureFlags() {
  cl_->AppendSwitchASCII(switches::kEnableFeatures,
                         base::JoinString(enabled_features_, ","));
  cl_->AppendSwitchASCII(switches::kDisableFeatures,
                         base::JoinString(disabled_features_, ","));
}

void ScopedAddFeatureFlags::EnableIfNotSet(const base::Feature& feature) {
  AddFeatureIfNotSet(feature, true /* enable */);
}

void ScopedAddFeatureFlags::DisableIfNotSet(const base::Feature& feature) {
  AddFeatureIfNotSet(feature, false /* enable */);
}

void ScopedAddFeatureFlags::AddFeatureIfNotSet(const base::Feature& feature,
                                               bool enable) {
  const char* feature_name = feature.name;
  if (base::Contains(enabled_features_, feature_name) ||
      base::Contains(disabled_features_, feature_name)) {
    return;
  }
  if (enable) {
    enabled_features_.emplace_back(feature_name);
  } else {
    disabled_features_.emplace_back(feature_name);
  }
}

}  // namespace android_webview
