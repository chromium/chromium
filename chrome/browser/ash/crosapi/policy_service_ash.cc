// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/policy_service_ash.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"

namespace crosapi {

PolicyServiceAsh::PolicyServiceAsh() = default;

PolicyServiceAsh::~PolicyServiceAsh() = default;

void PolicyServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::PolicyService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PolicyServiceAsh::ReloadPolicy() {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  auto* policy_service = profile->GetProfilePolicyConnector()->policy_service();
  policy_service->RefreshPolicies(base::DoNothing(),
                                  policy::PolicyFetchReason::kLacros);
}

}  // namespace crosapi
