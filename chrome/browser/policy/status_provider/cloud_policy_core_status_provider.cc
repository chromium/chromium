// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/cloud_policy_core_status_provider.h"

#include "components/policy/core/common/cloud/cloud_policy_core.h"

CloudPolicyCoreStatusProvider::CloudPolicyCoreStatusProvider(
    policy::CloudPolicyCore* core)
    : core_(core) {
  core_->store()->AddObserver(this);
  // TODO(bartfab): Add an observer that watches for client errors. Observing
  // core_->client() directly is not safe as the client may be destroyed and
  // (re-)created anytime if the user signs in or out on desktop platforms.
}

CloudPolicyCoreStatusProvider::~CloudPolicyCoreStatusProvider() {
  core_->store()->RemoveObserver(this);
}

void CloudPolicyCoreStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void CloudPolicyCoreStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}
