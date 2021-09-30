// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include "base/check.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
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
    CloudPolicyStore* cloud_policy_store,
    std::unique_ptr<enterprise_signals::DeviceInfoFetcher> device_info_fetcher)
    : dm_token_storage_(dm_token_storage),
      cloud_policy_store_(cloud_policy_store),
      device_info_fetcher_(std::move(device_info_fetcher)) {
  DCHECK(dm_token_storage_);
  DCHECK(cloud_policy_store_);
  DCHECK(device_info_fetcher_);
}

BrowserSignalsDecorator::~BrowserSignalsDecorator() = default;

void BrowserSignalsDecorator::Decorate(DeviceTrustSignals& signals) {
  signals.set_device_id(dm_token_storage_->RetrieveClientId());

  if (cloud_policy_store_->has_policy()) {
    signals.set_obfuscated_customer_id(
        cloud_policy_store_->policy()->obfuscated_customer_id());
  }

  // TODO(b/178421844): Fetch these signals at construction time and cache
  // them.
  enterprise_signals::DeviceInfo device_info = device_info_fetcher_->Fetch();
  signals.set_serial_number(device_info.serial_number);
  absl::optional<bool> is_disk_encrypted =
      enterprise_signals::SettingValueToBool(device_info.disk_encrypted);
  if (is_disk_encrypted.has_value()) {
    signals.set_is_disk_encrypted(is_disk_encrypted.value());
  }
}

}  // namespace enterprise_connectors
