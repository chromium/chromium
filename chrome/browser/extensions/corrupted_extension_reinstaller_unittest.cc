// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/simple_test_tick_clock.h"
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
    CorruptedExtensionReinstaller::set_reinstall_action_for_test(&action_);
  }

  TestReinstallerTracker(const TestReinstallerTracker&) = delete;
  TestReinstallerTracker& operator=(const TestReinstallerTracker&) = delete;

  ~TestReinstallerTracker() {
    CorruptedExtensionReinstaller::set_reinstall_action_for_test(nullptr);
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
  std::optional<base::OnceClosure> saved_callback_;
  CorruptedExtensionReinstaller::ReinstallCallback action_;
};

using CorruptedExtensionReinstallerUnittest = ExtensionServiceTestBase;

// Tests that a single extension corruption will keep retrying reinstallation.
TEST_F(CorruptedExtensionReinstallerUnittest, Retry) {
  // Reinstaller depends on the extension service.
  InitializeEmptyExtensionService();

  CorruptedExtensionReinstaller reinstaller(profile());
  reinstaller.ExpectReinstallForCorruption(
      kDummyExtensionId,
      CorruptedExtensionReinstaller::PolicyReinstallReason::
          CORRUPTION_DETECTED_WEBSTORE,
      mojom::ManifestLocation::kInternal);

  TestReinstallerTracker tracker;

  reinstaller.NotifyExtensionDisabledDueToCorruption();
  EXPECT_EQ(1, tracker.call_count());
  tracker.Proceed();
  EXPECT_EQ(2, tracker.call_count());
  tracker.Proceed();
  EXPECT_EQ(3, tracker.call_count());
}

// Tests that CorruptedExtensionReinstaller doesn't schedule a
// CheckForExternalUpdates() when one is already in-flight through PostTask.
TEST_F(CorruptedExtensionReinstallerUnittest,
       DoNotScheduleWhenAlreadyInflight) {
  // Reinstaller depends on the extension service.
  InitializeEmptyExtensionService();

  CorruptedExtensionReinstaller reinstaller(profile_.get());
  reinstaller.ExpectReinstallForCorruption(
      kDummyExtensionId,
      CorruptedExtensionReinstaller::PolicyReinstallReason::
          CORRUPTION_DETECTED_WEBSTORE,
      mojom::ManifestLocation::kInternal);

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
