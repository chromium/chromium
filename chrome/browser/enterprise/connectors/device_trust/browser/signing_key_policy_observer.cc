// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/browser/signing_key_policy_observer.h"

#include "base/check.h"
#include "components/enterprise/device_trust/core/common_types.h"
#include "components/enterprise/device_trust/core/device_trust_key_manager.h"

namespace enterprise_connectors {

SigningKeyPolicyObserver::SigningKeyPolicyObserver(
    DeviceTrustKeyManager* browser_key_manager)
    : browser_key_manager_(browser_key_manager) {
  CHECK(browser_key_manager_);
}

SigningKeyPolicyObserver::~SigningKeyPolicyObserver() = default;

void SigningKeyPolicyObserver::OnInlinePolicyEnabled(DTCPolicyLevel level) {
  if (level == DTCPolicyLevel::kBrowser) {
    browser_key_manager_->StartInitialization();
  }
}

}  // namespace enterprise_connectors
