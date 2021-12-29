// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"
#include "chrome/browser/ash/borealis/borealis_engagement_metrics.h"
#include "chrome/browser/ash/borealis/borealis_game_mode_controller.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_power_controller.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"
#include "components/exo/shell_surface_util.h"
#include "url/gurl.h"

namespace borealis {

namespace {

// Similar to a delayed callback, but can be cancelled by deleting.
class ScopedDelayedCallback {
 public:
  explicit ScopedDelayedCallback(base::OnceClosure callback,
                                 base::TimeDelta delay)
      : weak_factory_(this) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ScopedDelayedCallback::OnComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        delay);
  }

 private:
  void OnComplete(base::OnceClosure callback) { std::move(callback).Run(); }

  base::WeakPtrFactory<ScopedDelayedCallback> weak_factory_;
};

}  // namespace

class BorealisLifetimeObserver
    : public BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  explicit BorealisLifetimeObserver(Profile* profile)
      : profile_(profile), observation_(this), weak_factory_(this) {
    observation_.Observe(
        &BorealisService::GetForProfile(profile_)->WindowManager());
  }

  // BorealisWindowManager::AppWindowLifetimeObserver overrides.
  void OnSessionStarted() override {
    BorealisService::GetForProfile(profile_)
        ->ShutdownMonitor()
        .CancelDelayedShutdown();
  }
  void OnSessionFinished() override {
    BorealisService::GetForProfile(profile_)
        ->ShutdownMonitor()
        .ShutdownWithDelay();
  }

  void OnAppStarted(const std::string& app_id) override {
    app_delayers_.erase(app_id);
  }

  void OnAppFinished(const std::string& app_id,
                     aura::Window* last_window) override {
    // Launch post-game survey.
    // TODO(b/188745351): Remove this once it's no longer wanted.
    FeedbackFormUrl(
        profile_, app_id, base::UTF16ToUTF8(last_window->GetTitle()),
        base::BindOnce(&BorealisLifetimeObserver::OnFeedbackUrlGenerated,
                       weak_factory_.GetWeakPtr(), app_id));
  }

  void OnWindowManagerDeleted(BorealisWindowManager* window_manager) override {
    DCHECK(observation_.IsObservingSource(window_manager));
    observation_.Reset();
  }

 private:
  void OnFeedbackUrlGenerated(std::string app_id, GURL url) {
    if (url.is_valid()) {
      app_delayers_.emplace(
          app_id, std::make_unique<ScopedDelayedCallback>(
                      base::BindOnce(&BorealisLifetimeObserver::OnDelayComplete,
                                     weak_factory_.GetWeakPtr(), std::move(url),
                                     app_id),
                      base::Seconds(5)));
    }
  }

  void OnDelayComplete(GURL gurl, std::string app_id) {
    app_delayers_.erase(app_id);
    ash::NewWindowDelegate::GetInstance()->OpenUrl(
        gurl, /*from_user_interaction=*/true);
  }

  Profile* const profile_;
  base::ScopedObservation<BorealisWindowManager,
                          BorealisWindowManager::AppWindowLifetimeObserver>
      observation_;
  base::flat_map<std::string, std::unique_ptr<ScopedDelayedCallback>>
      app_delayers_;

  base::WeakPtrFactory<BorealisLifetimeObserver> weak_factory_;
};

// Borealis' main app extensively relies on self-activation, and it does not
// handle being refused that activation very well. This class exists to allow
// borealis' main app to self-activate at all times.
//
// TODO(b/190141156): Prevent crostini from spoofing borealis, which would allow
// it to self-activate its windows.  This would only be a problem currently on
// borealis-enabled systems, and only while borealis is running.
class SelfActivationPermissionGranter
    : public BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  explicit SelfActivationPermissionGranter(Profile* profile)
      : profile_(profile), observation_{this} {
    observation_.Observe(
        &BorealisService::GetForProfile(profile_)->WindowManager());
  }

  void OnWindowStarted(const std::string& app_id,
                       aura::Window* window) override {
    if (app_id == kClientAppId)
      exo::GrantPermissionToActivateIndefinitely(window);
  }

  void OnWindowFinished(const std::string& app_id,
                        aura::Window* window) override {
    if (app_id == kClientAppId)
      exo::RevokePermissionToActivate(window);
  }

  void OnWindowManagerDeleted(BorealisWindowManager* window_manager) override {
    DCHECK(observation_.IsObservingSource(window_manager));
    observation_.Reset();
  }

 private:
  Profile* const profile_;
  base::ScopedObservation<BorealisWindowManager,
                          BorealisWindowManager::AppWindowLifetimeObserver>
      observation_;
};

BorealisContext::~BorealisContext() = default;

void BorealisContext::SetDiskManagerForTesting(
    std::unique_ptr<BorealisDiskManager> disk_manager) {
  disk_manager_ = std::move(disk_manager);
}

void BorealisContext::NotifyUnexpectedVmShutdown() {
  guest_os_stability_monitor_->LogUnexpectedVmShutdown();
}

BorealisContext::BorealisContext(Profile* profile)
    : profile_(profile),
      lifetime_observer_(std::make_unique<BorealisLifetimeObserver>(profile)),
      guest_os_stability_monitor_(
          std::make_unique<guest_os::GuestOsStabilityMonitor>(
              kBorealisStabilityHistogram)),
      game_mode_controller_(std::make_unique<BorealisGameModeController>()),
      engagement_metrics_(std::make_unique<BorealisEngagementMetrics>(profile)),
      disk_manager_(std::make_unique<BorealisDiskManagerImpl>(this)),
      power_controller_(std::make_unique<BorealisPowerController>()),
      self_activation_granter_(
          std::make_unique<SelfActivationPermissionGranter>(profile)) {}

std::unique_ptr<BorealisContext>
BorealisContext::CreateBorealisContextForTesting(Profile* profile) {
  // Construct out-of-place because the constructor is private.
  BorealisContext* ptr = new BorealisContext(profile);
  return base::WrapUnique(ptr);
}

}  // namespace borealis
