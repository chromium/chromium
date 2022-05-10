// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include <functional>

#include "base/check.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

namespace {
using policy::BrowserDMTokenStorage;
using policy::CloudPolicyStore;

constexpr char kLatencyHistogramVariant[] = "Browser";
constexpr char kLatencyHistogramWithCacheVariant[] = "Browser.WithCache";
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

void BrowserSignalsDecorator::Decorate(base::Value::Dict& signals,
                                       base::OnceClosure done_closure) {
  auto start_time = base::TimeTicks::Now();

  signals.Set(device_signals::names::kDeviceId,
              dm_token_storage_->RetrieveClientId());

  if (cloud_policy_store_->has_policy()) {
    const auto* policy = cloud_policy_store_->policy();
    signals.Set(device_signals::names::kObfuscatedCustomerId,
                policy->obfuscated_customer_id());
    signals.Set(device_signals::names::kEnrollmentDomain,
                policy->has_managed_by() ? policy->managed_by()
                                         : policy->display_domain());
  }

  if (cache_initialized_) {
    UpdateFromCache(signals);
    LogSignalsCollectionLatency(kLatencyHistogramWithCacheVariant, start_time);
    std::move(done_closure).Run();
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&enterprise_signals::DeviceInfoFetcher::Fetch,
                     enterprise_signals::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&BrowserSignalsDecorator::OnDeviceInfoFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::ref(signals),
                     start_time, std::move(done_closure)));
}

void BrowserSignalsDecorator::OnDeviceInfoFetched(
    base::Value::Dict& signals,
    base::TimeTicks start_time,
    base::OnceClosure done_closure,
    const enterprise_signals::DeviceInfo& device_info) {
  cached_serial_number_ = device_info.serial_number;
  cached_is_disk_encrypted_ =
      enterprise_signals::SettingValueToBool(device_info.disk_encrypted);

  cache_initialized_ = true;
  UpdateFromCache(signals);

  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);

  std::move(done_closure).Run();
}

void BrowserSignalsDecorator::UpdateFromCache(base::Value::Dict& signals) {
  DCHECK(cache_initialized_);
  if (cached_serial_number_) {
    signals.Set(device_signals::names::kSerialNumber,
                cached_serial_number_.value());
  }

  if (cached_is_disk_encrypted_.has_value()) {
    signals.Set(device_signals::names::kIsDiskEncrypted,
                cached_is_disk_encrypted_.value());
  }
}

}  // namespace enterprise_connectors
