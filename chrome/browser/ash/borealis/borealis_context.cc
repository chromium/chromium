// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_engagement_metrics.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_power_controller.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"

namespace borealis {

namespace {

// Similar to a delayed callback, but can be cancelled by deleting.
class ScopedDelayedCallback {
 public:
  explicit ScopedDelayedCallback(base::OnceClosure callback,
                                 base::TimeDelta delay)
      : weak_factory_(this) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
  explicit BorealisLifetimeObserver(BorealisContext* context)
      : context_(context), observation_(this), weak_factory_(this) {
    observation_.Observe(
        &BorealisServiceFactory::GetForProfile(context_->profile())
             ->WindowManager());
  }

  // BorealisWindowManager::AppWindowLifetimeObserver overrides.
  void OnSessionStarted() override {
    if (!context_->launch_options().auto_shutdown)
      return;
    BorealisServiceFactory::GetForProfile(context_->profile())
        ->ShutdownMonitor()
        .CancelDelayedShutdown();
  }

  void OnSessionFinished() override {
    if (!context_->launch_options().auto_shutdown)
      return;
    BorealisServiceFactory::GetForProfile(context_->profile())
        ->ShutdownMonitor()
        .ShutdownWithDelay();
  }

  void OnAppStarted(const std::string& app_id) override {
    app_delayers_.erase(app_id);
  }

  void OnWindowManagerDeleted(BorealisWindowManager* window_manager) override {
    DCHECK(observation_.IsObservingSource(window_manager));
    observation_.Reset();
  }

 private:
  const raw_ptr<BorealisContext> context_;
  base::ScopedObservation<BorealisWindowManager,
                          BorealisWindowManager::AppWindowLifetimeObserver>
      observation_;
  base::flat_map<std::string, std::unique_ptr<ScopedDelayedCallback>>
      app_delayers_;

  base::WeakPtrFactory<BorealisLifetimeObserver> weak_factory_;
};

BorealisContext::~BorealisContext() = default;

void BorealisContext::NotifyUnexpectedVmShutdown() {
  guest_os_stability_monitor_->LogUnexpectedVmShutdown();
}

BorealisContext::BorealisContext(Profile* profile)
    : profile_(profile),
      lifetime_observer_(std::make_unique<BorealisLifetimeObserver>(this)),
      guest_os_stability_monitor_(
          std::make_unique<guest_os::GuestOsStabilityMonitor>(
              kBorealisStabilityHistogram)),
      engagement_metrics_(std::make_unique<BorealisEngagementMetrics>(profile)),
      power_controller_(std::make_unique<BorealisPowerController>(profile)) {}

std::unique_ptr<BorealisContext>
BorealisContext::CreateBorealisContextForTesting(Profile* profile) {
  // Construct out-of-place because the constructor is private.
  BorealisContext* ptr = new BorealisContext(profile);
  return base::WrapUnique(ptr);
}

}  // namespace borealis
