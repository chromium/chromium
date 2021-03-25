// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace enterprise_connectors {

DeviceTrustService::DeviceTrustService() = default;

DeviceTrustService::DeviceTrustService(Profile* profile)
    : prefs_(profile->GetPrefs()),
      first_report_sent_(false),
      signal_report_callback_(
          base::BindOnce(&DeviceTrustService::OnSignalReported,
                         base::Unretained(this))) {
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  key_pair_ = std::make_unique<DeviceTrustKeyPair>();
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  pref_observer_.Init(prefs_);
  pref_observer_.Add(kContextAwareAccessSignalsAllowlistPref,
                     base::BindRepeating(&DeviceTrustService::OnPolicyUpdated,
                                         base::Unretained(this)));
  // Using Unretained is ok here because pref_observer_ is owned by this class,
  // and we Remove() this path from pref_observer_ in Shutdown().
  reporter_ =
      std::make_unique<enterprise_connectors::DeviceTrustSignalReporter>();
}

DeviceTrustService::~DeviceTrustService() = default;

void DeviceTrustService::Shutdown() {
  pref_observer_.Remove(kContextAwareAccessSignalsAllowlistPref);
}

bool DeviceTrustService::IsEnabled() const {
  return (prefs_->HasPrefPath(kContextAwareAccessSignalsAllowlistPref) &&
          !prefs_->GetList(kContextAwareAccessSignalsAllowlistPref)->empty());
}

void DeviceTrustService::OnPolicyUpdated() {
  if (!reporter_) {
    return;
  }

  if (!first_report_sent_ &&
      IsEnabled()) {  // Policy enabled for the first time.
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
    key_pair_->Init();
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
    reporter_->Init(
        base::BindRepeating(
            [](DeviceTrustService* self) { return self->IsEnabled(); },
            base::Unretained(this)),
        // Unretained is ok here since owned by this class, and this callback is
        // only used in reporter_.SendReport().
        base::BindOnce(&DeviceTrustService::OnReporterInitialized,
                       weak_factory_.GetWeakPtr()));
  }
}

void DeviceTrustService::OnReporterInitialized(bool success) {
  if (!success) {
    // Initialization failed, so reset reporter_ to prevent retrying Init().
    reporter_.reset();
    return;
  }

  base::Value val(base::Value::Type::DICTIONARY);

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  val.SetStringKey("machine_attestion_key", key_pair_->ExportPEMPublicKey());
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  reporter_->SendReport(std::move(val), std::move(signal_report_callback_));
}

void DeviceTrustService::OnSignalReported(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to send device trust signal report.";
    // TODO(https://crbug.com/1186413) Handle failure cases.
  } else {
    first_report_sent_ = true;
  }
}

void DeviceTrustService::SetSignalReporterForTesting(
    std::unique_ptr<enterprise_connectors::DeviceTrustSignalReporter>
        reporter) {
  reporter_ = std::move(reporter);
}

void DeviceTrustService::SetSignalReportCallbackForTesting(
    base::OnceCallback<void(bool)> cb) {
  signal_report_callback_ = std::move(cb);
}

}  // namespace enterprise_connectors
