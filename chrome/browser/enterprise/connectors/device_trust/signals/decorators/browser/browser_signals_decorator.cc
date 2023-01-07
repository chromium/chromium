// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include <functional>
#include <utility>

#include "base/check.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_utils.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

namespace {
using policy::CloudPolicyStore;

constexpr char kLatencyHistogramVariant[] = "Browser";
}  // namespace

BrowserSignalsDecorator::BrowserSignalsDecorator(
    CloudPolicyStore* cloud_policy_store)
    : cloud_policy_store_(cloud_policy_store) {
  DCHECK(cloud_policy_store_);
}

BrowserSignalsDecorator::~BrowserSignalsDecorator() = default;

void BrowserSignalsDecorator::Decorate(base::Value::Dict& signals,
                                       base::OnceClosure done_closure) {
  auto start_time = base::TimeTicks::Now();

  if (cloud_policy_store_->has_policy()) {
    const auto* policy = cloud_policy_store_->policy();
    signals.Set(device_signals::names::kDeviceEnrollmentDomain,
                policy->has_managed_by() ? policy->managed_by()
                                         : policy->display_domain());
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
  signals.Set(device_signals::names::kSerialNumber, device_info.serial_number);
  signals.Set(device_signals::names::kScreenLockSecured,
              static_cast<int32_t>(device_info.screen_lock_secured));
  signals.Set(device_signals::names::kDiskEncrypted,
              static_cast<int32_t>(device_info.disk_encrypted));
  signals.Set(device_signals::names::kDeviceHostName,
              device_info.device_host_name);
  signals.Set(device_signals::names::kMacAddresses,
              ToListValue(device_info.mac_addresses));

  if (device_info.windows_machine_domain) {
    signals.Set(device_signals::names::kWindowsMachineDomain,
                device_info.windows_machine_domain.value());
  }

  if (device_info.windows_user_domain) {
    signals.Set(device_signals::names::kWindowsUserDomain,
                device_info.windows_user_domain.value());
  }

  if (device_info.secure_boot_enabled) {
    signals.Set(device_signals::names::kSecureBootEnabled,
                static_cast<int32_t>(device_info.secure_boot_enabled.value()));
  }

  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);

  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
