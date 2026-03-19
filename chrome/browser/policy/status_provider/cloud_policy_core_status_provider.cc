// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/cloud_policy_core_status_provider.h"

#include "components/policy/core/common/cloud/cloud_policy_core.h"

CloudPolicyCoreStatusProvider::CloudPolicyCoreStatusProvider(
    policy::CloudPolicyCore* core,
    policy::CloudPolicyCore* extension_install_core)
    : core_(core), extension_install_core_(extension_install_core) {
  scoped_observation_.Observe(core_->store());
  if (extension_install_core_ && extension_install_core_->store()) {
    extension_install_core_scoped_observation_.Observe(
        extension_install_core_->store());
  }
  // TODO(bartfab): Add an observer that watches for client errors. Observing
  // core_->client() directly is not safe as the client may be destroyed and
  // (re-)created anytime if the user signs in or out on desktop platforms.
}

CloudPolicyCoreStatusProvider::~CloudPolicyCoreStatusProvider() = default;

void CloudPolicyCoreStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void CloudPolicyCoreStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void CloudPolicyCoreStatusProvider::OnStoreDestruction(
    policy::CloudPolicyStore* store) {
  scoped_observation_.Reset();
  if (extension_install_core_) {
    extension_install_core_scoped_observation_.Reset();
  }
}
