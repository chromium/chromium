// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/force_installed_tracker_lacros.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_test_base.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::ForceInstalledTracker;
using ExtensionStatus = ForceInstalledTracker::ExtensionStatus;

class FakeForceInstalledTrackerLacros : public ForceInstalledTrackerLacros {
 public:
  FakeForceInstalledTrackerLacros() = default;
  ~FakeForceInstalledTrackerLacros() override = default;

  // ForceInstalledTrackerLacros:
  void OnForceInstalledExtensionsReady() override { is_ready_ = true; }

  // Skip the check of service availability for testing.
  bool IsServiceAvailable() const override { return true; }

  // Use a plug-in `ForceInstalledTracker` instance for testing.
  ForceInstalledTracker* GetExtensionForceInstalledTracker() override {
    return tracker_;
  }

  bool is_ready() const { return is_ready_; }

  void set_tracker(ForceInstalledTracker* tracker) { tracker_ = tracker; }

 private:
  bool is_ready_ = false;
  raw_ptr<ForceInstalledTracker> tracker_;
};

class ForceInstalledTrackerLacrosTest
    : public extensions::ForceInstalledTestBase {
 public:
  ForceInstalledTrackerLacrosTest() = default;
  ForceInstalledTrackerLacrosTest(const ForceInstalledTrackerLacrosTest&) =
      delete;
  ForceInstalledTrackerLacrosTest& operator=(
      const ForceInstalledTrackerLacrosTest&) = delete;

  FakeForceInstalledTrackerLacros* tracker_in_lacros() {
    return &tracker_in_lacros_;
  }

  void InitialAndStartTrackerInLacros() {
    tracker_in_lacros_.set_tracker(force_installed_tracker());
    tracker_in_lacros_.Start();
  }

 private:
  FakeForceInstalledTrackerLacros tracker_in_lacros_;
};

TEST_F(ForceInstalledTrackerLacrosTest, UnavailableTracker) {
  SetupEmptyForceList();
  tracker_in_lacros()->set_tracker(nullptr);
  tracker_in_lacros()->Start();
  EXPECT_FALSE(tracker_in_lacros()->is_ready());
}

TEST_F(ForceInstalledTrackerLacrosTest, EmptyForceList) {
  SetupEmptyForceList();
  InitialAndStartTrackerInLacros();
  EXPECT_TRUE(tracker_in_lacros()->is_ready());
}

TEST_F(ForceInstalledTrackerLacrosTest, ForceList) {
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kPending);

  SetupForceList(ExtensionOrigin::kWebStore);
  InitialAndStartTrackerInLacros();
  EXPECT_FALSE(tracker_in_lacros()->is_ready());

  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionLoaded(profile(), ext2.get());
  EXPECT_FALSE(tracker_in_lacros()->is_ready());

  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  force_installed_tracker()->OnExtensionReady(profile(), ext2.get());
  EXPECT_TRUE(tracker_in_lacros()->is_ready());
}
