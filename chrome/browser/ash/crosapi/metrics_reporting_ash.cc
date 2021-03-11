// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace crosapi {
namespace {

// Delegate for production, which uses the real reporting subsystem.
class DelegateImpl : public MetricsReportingAsh::Delegate {
 public:
  DelegateImpl() = default;
  DelegateImpl(const DelegateImpl&) = delete;
  DelegateImpl& operator=(const DelegateImpl&) = delete;
  ~DelegateImpl() override = default;

  // MetricsReportingAsh::Delegate:
  void SetMetricsReportingEnabled(bool enabled) override {
    // Use primary profile because Lacros does not support multi-signin.
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    // Chrome OS uses this wrapper around the underlying metrics pref.
    ash::StatsReportingController::Get()->SetEnabled(profile, enabled);
  }
};

}  // namespace

MetricsReportingAsh::MetricsReportingAsh(PrefService* local_state)
    : MetricsReportingAsh(std::make_unique<DelegateImpl>(), local_state) {}

MetricsReportingAsh::MetricsReportingAsh(std::unique_ptr<Delegate> delegate,
                                         PrefService* local_state)
    : delegate_(std::move(delegate)), local_state_(local_state) {
  DCHECK(delegate_);
  DCHECK(local_state_);
  pref_change_registrar_.Init(local_state_);
  // base::Unretained() is safe because PrefChangeRegistrar removes all
  // observers when it is destroyed.
  pref_change_registrar_.Add(
      metrics::prefs::kMetricsReportingEnabled,
      base::BindRepeating(&MetricsReportingAsh::NotifyObservers,
                          base::Unretained(this)));
}

MetricsReportingAsh::~MetricsReportingAsh() = default;

void MetricsReportingAsh::BindReceiver(
    mojo::PendingReceiver<mojom::MetricsReporting> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MetricsReportingAsh::AddObserver(
    mojo::PendingRemote<mojom::MetricsReportingObserver> observer) {
  mojo::Remote<mojom::MetricsReportingObserver> remote(std::move(observer));
  // Fire the observer with the initial value.
  bool enabled = IsMetricsReportingEnabled();
  remote->OnMetricsReportingChanged(enabled);
  // Store the observer for future notifications.
  observers_.Add(std::move(remote));
}

void MetricsReportingAsh::SetMetricsReportingEnabled(
    bool enabled,
    SetMetricsReportingEnabledCallback callback) {
  delegate_->SetMetricsReportingEnabled(enabled);
  std::move(callback).Run();
}

void MetricsReportingAsh::NotifyObservers() {
  bool enabled = IsMetricsReportingEnabled();
  for (auto& observer : observers_) {
    observer->OnMetricsReportingChanged(enabled);
  }
}

bool MetricsReportingAsh::IsMetricsReportingEnabled() const {
  return local_state_->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

}  // namespace crosapi
