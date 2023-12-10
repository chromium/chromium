// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"

#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"

namespace crosapi {
namespace {

// Delegate for production, which uses the real reporting subsystem.
class DelegateImpl : public MetricsReportingAsh::Delegate {
 public:
  explicit DelegateImpl(metrics::MetricsService* metrics_service)
      : metrics_service_(metrics_service) {
    DCHECK(metrics_service_);
  }
  DelegateImpl(const DelegateImpl&) = delete;
  DelegateImpl& operator=(const DelegateImpl&) = delete;
  ~DelegateImpl() override = default;

  bool IsMetricsReportingEnabled() override {
    return metrics_service_->IsMetricsReportingEnabled();
  }

  // MetricsReportingAsh::Delegate:
  void SetMetricsReportingEnabled(bool enabled) override {
    // Use primary profile because Lacros does not support multi-signin.
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    // Chrome OS uses this wrapper around the underlying metrics pref.
    ash::StatsReportingController::Get()->SetEnabled(profile, enabled);
  }

  std::string GetClientId() override { return metrics_service_->GetClientId(); }

  base::CallbackListSubscription AddEnablementObserver(
      const base::RepeatingCallback<void(bool)>& observer) override {
    return metrics_service_->AddEnablementObserver(observer);
  }

 private:
  const raw_ptr<metrics::MetricsService, DanglingUntriaged> metrics_service_;
};

std::optional<std::string> MaybeGetClientId(
    bool enabled,
    MetricsReportingAsh::Delegate* delegate) {
  return enabled ? std::make_optional(delegate->GetClientId()) : std::nullopt;
}

}  // namespace

std::unique_ptr<MetricsReportingAsh>
MetricsReportingAsh::CreateMetricsReportingAsh(
    metrics::MetricsService* metrics_service) {
  DCHECK(metrics_service);
  return std::make_unique<MetricsReportingAsh>(
      std::make_unique<DelegateImpl>(metrics_service));
}

MetricsReportingAsh::MetricsReportingAsh(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  observer_subscription_ = delegate_->AddEnablementObserver(base::BindRepeating(
      &MetricsReportingAsh::OnEnablementChange, base::Unretained(this)));
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
  bool enabled = delegate_->IsMetricsReportingEnabled();
  remote->OnMetricsReportingChanged(enabled,
                                    MaybeGetClientId(enabled, delegate_.get()));
  // Store the observer for future notifications.
  observers_.Add(std::move(remote));
}

void MetricsReportingAsh::SetMetricsReportingEnabled(
    bool enabled,
    SetMetricsReportingEnabledCallback callback) {
  delegate_->SetMetricsReportingEnabled(enabled);
  std::move(callback).Run();
}

void MetricsReportingAsh::OnEnablementChange(bool enabled) {
  for (auto& observer : observers_) {
    observer->OnMetricsReportingChanged(
        enabled, MaybeGetClientId(enabled, delegate_.get()));
  }
}

}  // namespace crosapi
