// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include "base/check.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

namespace {
using policy::BrowserDMTokenStorage;
using policy::CloudPolicyStore;
}  // namespace

BrowserSignalsDecorator::BrowserSignalsDecorator(
    BrowserDMTokenStorage* dm_token_storage,
    CloudPolicyStore* cloud_policy_store)
    : dm_token_storage_(dm_token_storage),
      cloud_policy_store_(cloud_policy_store) {
  DCHECK(dm_token_storage_);
  DCHECK(cloud_policy_store_);
}

BrowserSignalsDecorator::~BrowserSignalsDecorator() = default;

void BrowserSignalsDecorator::Decorate(DeviceTrustSignals& signals) {
  signals.set_device_id(dm_token_storage_->RetrieveClientId());

  if (!cloud_policy_store_->has_policy()) {
    return;
  }
  signals.set_obfuscated_customer_id(
      cloud_policy_store_->policy()->obfuscated_customer_id());
}

}  // namespace enterprise_connectors
