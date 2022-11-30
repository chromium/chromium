// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/fake_plugin_vm_features.h"

namespace plugin_vm {

FakePluginVmFeatures::FakePluginVmFeatures() {
  original_features_ = PluginVmFeatures::Get();
  PluginVmFeatures::SetForTesting(this);
}

FakePluginVmFeatures::~FakePluginVmFeatures() {
  PluginVmFeatures::SetForTesting(original_features_);
}

bool FakePluginVmFeatures::IsAllowed(const Profile* profile,
                                     std::string* reason) {
  if (allowed_.has_value()) {
    *reason = disallowed_reason_;
    return *allowed_;
  }
  return original_features_->IsAllowed(profile, reason);
}

bool FakePluginVmFeatures::IsConfigured(const Profile* profile) {
  if (configured_.has_value())
    return *configured_;
  return original_features_->IsConfigured(profile);
}

bool FakePluginVmFeatures::IsEnabled(const Profile* profile) {
  if (enabled_.has_value())
    return *enabled_;
  return original_features_->IsEnabled(profile);
}

}  // namespace plugin_vm
