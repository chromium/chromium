// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_extension_reinstaller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {

namespace {
const char kDummyExtensionId[] = "whatever";
}

class TestReinstallerTracker {
 public:
  TestReinstallerTracker()
      : action_(base::BindRepeating(&TestReinstallerTracker::ReinstallAction,
                                    base::Unretained(this))) {
    PolicyExtensionReinstaller::set_policy_reinstall_action_for_test(&action_);
  }
  ~TestReinstallerTracker() {
    PolicyExtensionReinstaller::set_policy_reinstall_action_for_test(nullptr);
  }
  void ReinstallAction(base::OnceClosure callback,
                       base::TimeDelta reinstall_delay) {
    ++call_count_;
    saved_callback_ = std::move(callback);
  }
  void Proceed() {
    DCHECK(saved_callback_);
    DCHECK(!saved_callback_->is_null());
    // Run() will set |saved_callback_| again, so use a temporary.
    base::OnceClosure callback = std::move(saved_callback_.value());
    saved_callback_.reset();
    std::move(callback).Run();
  }
  int call_count() { return call_count_; }

 private:
  int call_count_ = 0;
  absl::optional<base::OnceClosure> saved_callback_;
  PolicyExtensionReinstaller::ReinstallCallback action_;

  DISALLOW_COPY_AND_ASSIGN(TestReinstallerTracker);
};

using PolicyExtensionReinstallerUnittest = ExtensionServiceTestBase;

// Tests that a single extension corruption will keep retrying reinstallation.
TEST_F(PolicyExtensionReinstallerUnittest, Retry) {
  InitializeEmptyExtensionService();
  service()->pending_extension_manager()->ExpectReinstallForCorruption(
      kDummyExtensionId,
      PendingExtensionManager::PolicyReinstallReason::
          CORRUPTION_DETECTED_WEBSTORE,
      mojom::ManifestLocation::kInternal);

  PolicyExtensionReinstaller reinstaller(profile_.get());
  TestReinstallerTracker tracker;

  reinstaller.NotifyExtensionDisabledDueToCorruption();
  EXPECT_EQ(1, tracker.call_count());
  tracker.Proceed();
  EXPECT_EQ(2, tracker.call_count());
  tracker.Proceed();
  EXPECT_EQ(3, tracker.call_count());
}

// Tests that PolicyExtensionReinstaller doesn't schedule a
// CheckForExternalUpdates() when one is already in-flight through PostTask.
TEST_F(PolicyExtensionReinstallerUnittest, DoNotScheduleWhenAlreadyInflight) {
  InitializeEmptyExtensionService();
  service()->pending_extension_manager()->ExpectReinstallForCorruption(
      kDummyExtensionId,
      PendingExtensionManager::PolicyReinstallReason::
          CORRUPTION_DETECTED_WEBSTORE,
      mojom::ManifestLocation::kInternal);

  PolicyExtensionReinstaller reinstaller(profile_.get());
  TestReinstallerTracker tracker;

  reinstaller.NotifyExtensionDisabledDueToCorruption();
  EXPECT_EQ(1, tracker.call_count());
  reinstaller.NotifyExtensionDisabledDueToCorruption();
  // Resolve the reinstall attempt.
  tracker.Proceed();
  EXPECT_EQ(2, tracker.call_count());
  reinstaller.NotifyExtensionDisabledDueToCorruption();
  // Not resolving the pending attempt will not produce further calls.
  EXPECT_EQ(2, tracker.call_count());
}

}  // namespace extensions
