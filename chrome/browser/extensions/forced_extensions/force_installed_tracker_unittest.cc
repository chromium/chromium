// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"

#include "base/scoped_observer.h"
#include "base/values.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_test_base.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ForceInstalledTrackerTest : public ForceInstalledTestBase,
                                  public ForceInstalledTracker::Observer {
 public:
  ForceInstalledTrackerTest() = default;

  ForceInstalledTrackerTest(const ForceInstalledTrackerTest&) = delete;
  ForceInstalledTrackerTest& operator=(const ForceInstalledTrackerTest&) =
      delete;

  void SetUp() override {
    ForceInstalledTestBase::SetUp();
    scoped_observer_.Add(force_installed_tracker());
  }

  // ForceInstalledTracker::Observer overrides:
  void OnForceInstalledExtensionsLoaded() override { loaded_called_ = true; }
  void OnForceInstalledExtensionsReady() override { ready_called_ = true; }

 protected:
  ScopedObserver<ForceInstalledTracker, ForceInstalledTracker::Observer>
      scoped_observer_{this};
  bool loaded_called_ = false;
  bool ready_called_ = false;
};

TEST_F(ForceInstalledTrackerTest, EmptyForcelist) {
  SetupEmptyForceList();
  EXPECT_TRUE(loaded_called_);
  EXPECT_TRUE(ready_called_);
}

TEST_F(ForceInstalledTrackerTest, BeforeForceInstallPolicy) {
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  SetupForceList();
}

// This test verifies that OnForceInstalledExtensionsLoaded() is called once all
// force installed extensions have successfully loaded and
// OnForceInstalledExtensionsReady() is called once all those extensions have
// become ready for use.
TEST_F(ForceInstalledTrackerTest, AllExtensionsInstalled) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_FALSE(force_installed_tracker()->IsDoneLoading());

  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionLoaded(profile(), ext2.get());
  EXPECT_TRUE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
  EXPECT_FALSE(force_installed_tracker()->IsReady());

  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  force_installed_tracker()->OnExtensionReady(profile(), ext2.get());
  EXPECT_TRUE(loaded_called_);
  EXPECT_TRUE(ready_called_);
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
  EXPECT_TRUE(force_installed_tracker()->IsReady());
}

// This test verifies that OnForceInstalledExtensionsLoaded() is not called till
// all extensions have either successfully loaded or failed.
TEST_F(ForceInstalledTrackerTest, ExtensionPendingInstall) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_FALSE(force_installed_tracker()->IsDoneLoading());

  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_FALSE(force_installed_tracker()->IsDoneLoading());
}

// This test verifies that applying a new policy value for force installed
// extensions once all the extensions in the previous value have loaded does not
// trigger the observers again.
TEST_F(ForceInstalledTrackerTest, ObserversOnlyCalledOnce) {
  // Start with a non-empty force-list, and install them, which triggers
  // observer.
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionLoaded(profile(), ext2.get());
  EXPECT_TRUE(loaded_called_);

  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  force_installed_tracker()->OnExtensionReady(profile(), ext2.get());
  EXPECT_TRUE(ready_called_);

  SetupEmptyForceList();
  EXPECT_TRUE(loaded_called_);
  EXPECT_TRUE(ready_called_);
}

// This test verifies that observer is called if force installed extensions are
// either successfully loaded or failed.
TEST_F(ForceInstalledTrackerTest, ExtensionsInstallationFailed) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionInstallationFailed(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  EXPECT_TRUE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
}

// This test tracks the status of the force installed extensions in
// |ForceInstalledTracker::extensions_| as the extensions are either loaded or
// failed.
TEST_F(ForceInstalledTrackerTest, ExtensionsStatus) {
  SetupForceList();
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId1).status,
            ForceInstalledTracker::ExtensionStatus::PENDING);
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId2).status,
            ForceInstalledTracker::ExtensionStatus::PENDING);

  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionInstallationFailed(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId1).status,
            ForceInstalledTracker::ExtensionStatus::LOADED);
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId2).status,
            ForceInstalledTracker::ExtensionStatus::FAILED);

  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId1).status,
            ForceInstalledTracker::ExtensionStatus::READY);
}

// This test verifies that resetting the policy before all force installed
// extensions are either loaded or failed does not call the observers.
TEST_F(ForceInstalledTrackerTest, ExtensionsInstallationCancelled) {
  SetupForceList();
  SetupEmptyForceList();
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
}

// This test verifies that READY state observer is called when each force
// installed extension is either ready for use or failed.
TEST_F(ForceInstalledTrackerTest, AllExtensionsReady) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  force_installed_tracker()->OnExtensionInstallationFailed(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  EXPECT_TRUE(loaded_called_);
  EXPECT_TRUE(ready_called_);
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
}

}  // namespace extensions
