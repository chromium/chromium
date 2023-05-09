// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/consent_policy_observer.h"

#include "base/check.h"
#include "components/device_signals/core/browser/user_permission_service.h"

namespace enterprise_connectors {

ConsentPolicyObserver::ConsentPolicyObserver(
    base::WeakPtr<device_signals::UserPermissionService>
        user_permission_service)
    : user_permission_service_(std::move(user_permission_service)) {
  CHECK(user_permission_service_);
}

ConsentPolicyObserver::~ConsentPolicyObserver() = default;

void ConsentPolicyObserver::OnInlinePolicyDisabled(DTCPolicyLevel level) {
  if (level == DTCPolicyLevel::kUser && user_permission_service_) {
    // User consent is only affected by changes in user-level policies.
    user_permission_service_->ResetUserConsentIfNeeded();
  }
}
}  // namespace enterprise_connectors
