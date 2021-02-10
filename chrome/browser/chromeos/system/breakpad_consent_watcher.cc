// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/breakpad_consent_watcher.h"

#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "components/crash/core/app/crashpad.h"

namespace chromeos {
namespace system {

BreakpadConsentWatcher::BreakpadConsentWatcher() = default;
BreakpadConsentWatcher::~BreakpadConsentWatcher() = default;

std::unique_ptr<BreakpadConsentWatcher> BreakpadConsentWatcher::Initialize(
    StatsReportingController* stat_controller) {
  DCHECK(stat_controller != nullptr);
  if (crash_reporter::IsCrashpadEnabled()) {
    // Crashpad is always installed, regardless of consent. (crash_reporter is
    // responsible for discarding the reports if consent is off.) Therefore, we
    // do not need to watch the consent setting.
    return nullptr;
  }

  if (breakpad::IsCrashReporterEnabled()) {
    // Already enabled, no need to enable it again. (If consent is revoked,
    // crash_reporter will discard any resulting crashes.)
    return nullptr;
  }

  auto watcher = base::WrapUnique(new BreakpadConsentWatcher);
  watcher->subscription_ = stat_controller->AddObserver(
      base::BindRepeating(BreakpadConsentWatcher::OnConsentChange));

  return watcher;
}

void BreakpadConsentWatcher::OnConsentChange() {
  GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BreakpadConsentWatcher::OnConsentChangeCollectStatsConsentThread));
}

void BreakpadConsentWatcher::OnConsentChangeCollectStatsConsentThread() {
  if (breakpad::IsCrashReporterEnabled()) {
    // No need to enable breakpad twice.
    return;
  }

  // Breakpad will check the consent setting in InitCrashReporter. No need to
  // check it here.
  breakpad::InitCrashReporter(std::string());
}

}  // namespace system
}  // namespace chromeos
