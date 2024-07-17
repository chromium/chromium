// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"

#include <map>

#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_test_base.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using ExtensionStatus = ForceInstalledTracker::ExtensionStatus;
using ExtensionOrigin = ForceInstalledTestBase::ExtensionOrigin;

class ForceInstalledTrackerTest : public ForceInstalledTestBase,
                                  public ForceInstalledTracker::Observer {
 public:
  ForceInstalledTrackerTest() = default;

  ForceInstalledTrackerTest(const ForceInstalledTrackerTest&) = delete;
  ForceInstalledTrackerTest& operator=(const ForceInstalledTrackerTest&) =
      delete;

  void SetUp() override {
    ForceInstalledTestBase::SetUp();
    scoped_observation_.Observe(force_installed_tracker());
  }

  // ForceInstalledTracker::Observer overrides:
  void OnForceInstalledExtensionsLoaded() override { loaded_called_ = true; }
  void OnForceInstalledExtensionsReady() override { ready_called_ = true; }
  void OnForceInstalledExtensionFailed(
      const ExtensionId& extension_id,
      InstallStageTracker::FailureReason reason,
      bool is_from_store) override {
    error_reason_[extension_id] = reason;
  }

 protected:
  base::ScopedObservation<ForceInstalledTracker,
                          ForceInstalledTracker::Observer>
      scoped_observation_{this};
  bool loaded_called_ = false;
  bool ready_called_ = false;
  std::map<ExtensionId, InstallStageTracker::FailureReason> error_reason_;
};

TEST_F(ForceInstalledTrackerTest, EmptyForcelist) {
  SetupEmptyForceList();
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(error_reason_.empty());
  EXPECT_TRUE(force_installed_tracker()->IsReady());
  EXPECT_FALSE(force_installed_tracker()->IsComplete());
}

TEST_F(ForceInstalledTrackerTest, EmptyForcelistAndThenUpdated) {
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kPending);

  SetupEmptyForceList();
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(error_reason_.empty());

  SetupForceList(ExtensionOrigin::kWebStore);
  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionLoaded(profile(), ext2.get());
  EXPECT_TRUE(loaded_called_);
}

TEST_F(ForceInstalledTrackerTest, BeforeForceInstallPolicy) {
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(error_reason_.empty());
  SetupForceList(ExtensionOrigin::kWebStore);
}

// This test verifies that OnForceInstalledExtensionsLoaded() is called once all
// force installed extensions have successfully loaded and
// OnForceInstalledExtensionsReady() is called once all those extensions have
// become ready for use.
TEST_F(ForceInstalledTrackerTest, AllExtensionsInstalled) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kPending);
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(error_reason_.empty());
  EXPECT_FALSE(force_installed_tracker()->IsDoneLoading());

  force_installed_tracker()->OnExtensionLoaded(profile(), ext1.get());
  force_installed_tracker()->OnExtensionLoaded(profile(), ext2.get());
  EXPECT_TRUE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(error_reason_.empty());
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
  EXPECT_FALSE(force_installed_tracker()->IsReady());
  EXPECT_FALSE(force_installed_tracker()->IsComplete());

  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  force_installed_tracker()->OnExtensionReady(profile(), ext2.get());
  EXPECT_TRUE(loaded_called_);
  EXPECT_TRUE(ready_called_);
  EXPECT_TRUE(error_reason_.empty());
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
  EXPECT_TRUE(force_installed_tracker()->IsReady());
  EXPECT_TRUE(force_installed_tracker()->IsComplete());
}

// This test verifies that OnForceInstalledExtensionsLoaded() is not called till
// all extensions have either successfully loaded or failed.
TEST_F(ForceInstalledTrackerTest, ExtensionPendingInstall) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  force_installed_tracker()->OnExtensionInstallationFailed(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  EXPECT_TRUE(loaded_called_);
  EXPECT_FALSE(ready_called_);
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
  EXPECT_EQ(error_reason_.find(ExtensionId(kExtensionId1)),
            error_reason_.end());
  EXPECT_NE(error_reason_.find(ExtensionId(kExtensionId2)),
            error_reason_.end());
  EXPECT_EQ(error_reason_[kExtensionId2],
            InstallStageTracker::FailureReason::INVALID_ID);
}

// This test tracks the status of the force installed extensions in
// |ForceInstalledTracker::extensions_| as the extensions are either loaded or
// failed.
TEST_F(ForceInstalledTrackerTest, ExtensionsStatus) {
  SetupForceList(ExtensionOrigin::kWebStore);
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId1).status,
            ExtensionStatus::kPending);
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId2).status,
            ExtensionStatus::kPending);

  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  force_installed_tracker()->OnExtensionInstallationFailed(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId1).status,
            ExtensionStatus::kLoaded);
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId2).status,
            ExtensionStatus::kFailed);

  force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
  EXPECT_EQ(force_installed_tracker()->extensions().at(kExtensionId1).status,
            ExtensionStatus::kReady);
}

// This test verifies that resetting the policy before all force installed
// extensions are either loaded or failed does not call the observers.
TEST_F(ForceInstalledTrackerTest, ExtensionsInstallationCancelled) {
  SetupForceList(ExtensionOrigin::kWebStore);
  SetupEmptyForceList();
  EXPECT_FALSE(loaded_called_);
  EXPECT_FALSE(ready_called_);
}

// This test verifies that READY state observer is called when each force
// installed extension is either ready for use or failed.
TEST_F(ForceInstalledTrackerTest, AllExtensionsReady) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kReady);
  force_installed_tracker()->OnExtensionInstallationFailed(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  EXPECT_TRUE(loaded_called_);
  EXPECT_TRUE(ready_called_);
  EXPECT_TRUE(force_installed_tracker()->IsDoneLoading());
}

}  // namespace extensions
