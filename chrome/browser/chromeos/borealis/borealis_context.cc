// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/borealis/borealis_service.h"
#include "chrome/browser/chromeos/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/chromeos/borealis/borealis_window_manager.h"

namespace borealis {

class BorealisLifetimeObserver
    : public BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  explicit BorealisLifetimeObserver(Profile* profile)
      : profile_(profile), observation_{this} {
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

BorealisContext::BorealisContext(Profile* profile)
    : profile_(profile),
      lifetime_observer_(std::make_unique<BorealisLifetimeObserver>(profile)) {}

std::unique_ptr<BorealisContext>
BorealisContext::CreateBorealisContextForTesting(Profile* profile) {
  // Construct out-of-place because the constructor is private.
  BorealisContext* ptr = new BorealisContext(profile);
  return base::WrapUnique(ptr);
}

}  // namespace borealis
