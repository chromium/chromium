// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/version_info/version_info.h"

namespace enterprise_connectors {

namespace {

constexpr char kLatencyHistogramVariant[] = "Common";
constexpr char kLatencyHistogramWithCacheVariant[] = "Common.WithCache";

}  // namespace

CommonSignalsDecorator::CommonSignalsDecorator() = default;

CommonSignalsDecorator::~CommonSignalsDecorator() = default;

void CommonSignalsDecorator::Decorate(base::Value::Dict& signals,
                                      base::OnceClosure done_closure) {
  auto start_time = base::TimeTicks::Now();
  signals.Set(device_signals::names::kOs, policy::GetOSPlatform());
  signals.Set(device_signals::names::kOsVersion, policy::GetOSVersion());
  signals.Set(device_signals::names::kDisplayName, policy::GetDeviceName());
  signals.Set(device_signals::names::kBrowserVersion,
              version_info::GetVersionNumber());

  if (cached_device_model_ && cached_device_manufacturer_) {
    UpdateFromCache(signals);
    LogSignalsCollectionLatency(kLatencyHistogramWithCacheVariant, start_time);
    std::move(done_closure).Run();
    return;
  }

  auto callback =
      base::BindOnce(&CommonSignalsDecorator::OnHardwareInfoRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::ref(signals),
                     start_time, std::move(done_closure));

  base::SysInfo::GetHardwareInfo(std::move(callback));
}

void CommonSignalsDecorator::OnHardwareInfoRetrieved(
    base::Value::Dict& signals,
    base::TimeTicks start_time,
    base::OnceClosure done_closure,
    base::SysInfo::HardwareInfo hardware_info) {
  cached_device_model_ = hardware_info.model;
  cached_device_manufacturer_ = hardware_info.manufacturer;

  UpdateFromCache(signals);

  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);

  std::move(done_closure).Run();
}

void CommonSignalsDecorator::UpdateFromCache(base::Value::Dict& signals) {
  signals.Set(device_signals::names::kDeviceModel,
              cached_device_model_.value());
  signals.Set(device_signals::names::kDeviceManufacturer,
              cached_device_manufacturer_.value());
}

}  // namespace enterprise_connectors
