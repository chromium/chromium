// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

PrivacySandboxSettingsDelegate::PrivacySandboxSettingsDelegate(Profile* profile)
    : profile_(profile) {}

PrivacySandboxSettingsDelegate::~PrivacySandboxSettingsDelegate() = default;

bool PrivacySandboxSettingsDelegate::IsPrivacySandboxRestricted() {
  // When the Privacy Sandbox 3 feature is enabled, the Sandbox is restricted
  // for Child users.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3)) {
    // TODO(crbug.com/1286276): Integrate other identity provided signals.
    return profile_->IsChild();
  }
  // No restrictions apply otherwise.
  return false;
}
