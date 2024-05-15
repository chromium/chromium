// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/context_signals_decorator.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_utils.h"
#include "chrome/browser/enterprise/signals/context_info_fetcher.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace enterprise_connectors {

namespace {

constexpr char kLatencyHistogramVariant[] = "Context";

enum class PasswordProtectionTrigger {
  kUnset = 0,
  kOff = 1,
  kPasswordReuse = 2,
  kPhisingReuse = 3
};

PasswordProtectionTrigger ConvertPasswordProtectionTrigger(
    const std::optional<safe_browsing::PasswordProtectionTrigger>&
        policy_value) {
  if (!policy_value) {
    return PasswordProtectionTrigger::kUnset;
  }

  switch (policy_value.value()) {
    case safe_browsing::PASSWORD_PROTECTION_OFF:
      return PasswordProtectionTrigger::kOff;
    case safe_browsing::PASSWORD_REUSE:
      return PasswordProtectionTrigger::kPasswordReuse;
    case safe_browsing::PHISHING_REUSE:
      return PasswordProtectionTrigger::kPhisingReuse;
    case safe_browsing::PASSWORD_PROTECTION_TRIGGER_MAX:
      NOTREACHED_IN_MIGRATION();
      return PasswordProtectionTrigger::kUnset;
  }
}

}  // namespace

ContextSignalsDecorator::ContextSignalsDecorator(
    std::unique_ptr<enterprise_signals::ContextInfoFetcher>
        context_info_fetcher)
    : context_info_fetcher_(std::move(context_info_fetcher)) {
  DCHECK(context_info_fetcher_);
}

ContextSignalsDecorator::~ContextSignalsDecorator() = default;

void ContextSignalsDecorator::Decorate(base::Value::Dict& signals,
                                       base::OnceClosure done_closure) {
  context_info_fetcher_->Fetch(base::BindOnce(
      &ContextSignalsDecorator::OnSignalsFetched,
      weak_ptr_factory_.GetWeakPtr(), std::ref(signals),
      /*start_time=*/base::TimeTicks::Now(), std::move(done_closure)));
}

void ContextSignalsDecorator::OnSignalsFetched(
    base::Value::Dict& signals,
    base::TimeTicks start_time,
    base::OnceClosure done_closure,
    enterprise_signals::ContextInfo context_info) {
  signals.Set(device_signals::names::kDeviceAffiliationIds,
              ToListValue(context_info.browser_affiliation_ids));
  signals.Set(device_signals::names::kProfileAffiliationIds,
              ToListValue(context_info.profile_affiliation_ids));
  signals.Set(device_signals::names::kRealtimeUrlCheckMode,
              static_cast<int32_t>(context_info.realtime_url_check_mode));
  signals.Set(
      device_signals::names::kSafeBrowsingProtectionLevel,
      static_cast<int32_t>(context_info.safe_browsing_protection_level));
  signals.Set(device_signals::names::kSiteIsolationEnabled,
              context_info.site_isolation_enabled);
  signals.Set(device_signals::names::kPasswordProtectionWarningTrigger,
              static_cast<int32_t>(ConvertPasswordProtectionTrigger(
                  context_info.password_protection_warning_trigger)));
  signals.Set(device_signals::names::kChromeRemoteDesktopAppBlocked,
              context_info.chrome_remote_desktop_app_blocked);
  signals.Set(device_signals::names::kBuiltInDnsClientEnabled,
              context_info.built_in_dns_client_enabled);
  signals.Set(device_signals::names::kOsFirewall,
              static_cast<int32_t>(context_info.os_firewall));
  signals.Set(device_signals::names::kSystemDnsServers,
              ToListValue(context_info.system_dns_servers));

  if (context_info.third_party_blocking_enabled) {
    signals.Set(device_signals::names::kThirdPartyBlockingEnabled,
                context_info.third_party_blocking_enabled.value());
  }

  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);

  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
