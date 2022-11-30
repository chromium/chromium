// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_tracker.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/active_install_data.h"
#include "chrome/browser/extensions/scoped_active_install.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ActiveInstallData;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::InstallTracker;
using extensions::InstallObserver;
using extensions::ScopedActiveInstall;

namespace {

// Random extension ids for testing.
const char kExtensionId1[] = "oochhailbdickimldhhodijaldpllppf";
const char kExtensionId2[] = "ahionppacfhbbmpmlcbkdgcpokfpflji";
const char kExtensionId3[] = "ladmcjmmmmgonboiadnaindoekpbljde";

scoped_refptr<const Extension> CreateDummyExtension(const std::string& id) {
  return extensions::ExtensionBuilder("Dummy name")
      .SetLocation(extensions::mojom::ManifestLocation::kInternal)
      .SetID(id)
      .Build();
}

}  // namespace

class InstallTrackerTest : public testing::Test {
 public:
  InstallTrackerTest() {
    profile_ = std::make_unique<TestingProfile>();
    tracker_ = base::WrapUnique(new InstallTracker(profile_.get(), nullptr));
  }

  ~InstallTrackerTest() override {}

 protected:
  Profile* profile() { return profile_.get(); }
  InstallTracker* tracker() { return tracker_.get(); }

  void VerifyInstallData(const ActiveInstallData& original,
                         const ActiveInstallData& retrieved) {
    EXPECT_EQ(original.extension_id, retrieved.extension_id);
    EXPECT_EQ(original.percent_downloaded, retrieved.percent_downloaded);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<InstallTracker> tracker_;
};

// Verifies that active installs are registered and deregistered correctly.
TEST_F(InstallTrackerTest, AddAndRemoveActiveInstalls) {
  ActiveInstallData install_data1(kExtensionId1);
  install_data1.percent_downloaded = 76;
  ActiveInstallData install_data2(kExtensionId2);

  tracker_->AddActiveInstall(install_data1);
  tracker_->AddActiveInstall(install_data2);

  const ActiveInstallData* retrieved_data1 =
      tracker_->GetActiveInstall(kExtensionId1);
  const ActiveInstallData* retrieved_data2 =
      tracker_->GetActiveInstall(kExtensionId2);
  const ActiveInstallData* retrieved_data3 =
      tracker_->GetActiveInstall(kExtensionId3);
  ASSERT_TRUE(retrieved_data1);
  ASSERT_TRUE(retrieved_data2);
  ASSERT_FALSE(retrieved_data3);
  VerifyInstallData(install_data1, *retrieved_data1);
  VerifyInstallData(install_data2, *retrieved_data2);
  retrieved_data1 = nullptr;
  retrieved_data2 = nullptr;

  tracker_->RemoveActiveInstall(kExtensionId1);
  EXPECT_FALSE(tracker_->GetActiveInstall(kExtensionId1));
  EXPECT_TRUE(tracker_->GetActiveInstall(kExtensionId2));
  EXPECT_FALSE(tracker_->GetActiveInstall(kExtensionId3));
}

// Verifies that active installs are registered and deregistered correctly
// using ScopedActiveInstall.
TEST_F(InstallTrackerTest, ScopedActiveInstallDeregister) {
  // Verify the constructor that registers the install.
  ActiveInstallData install_data(kExtensionId1);
  install_data.percent_downloaded = 6;
  std::unique_ptr<ScopedActiveInstall> scoped_active_install(
      new ScopedActiveInstall(tracker(), install_data));

  const ActiveInstallData* retrieved_data =
      tracker_->GetActiveInstall(kExtensionId1);
  ASSERT_TRUE(retrieved_data);
  VerifyInstallData(install_data, *retrieved_data);
  retrieved_data = nullptr;

  scoped_active_install.reset();
  EXPECT_FALSE(tracker_->GetActiveInstall(kExtensionId1));

  // Verify the constructor that doesn't register the install.
  scoped_active_install =
      std::make_unique<ScopedActiveInstall>(tracker(), kExtensionId1);
  EXPECT_FALSE(tracker_->GetActiveInstall(kExtensionId1));

  tracker_->AddActiveInstall(install_data);
  EXPECT_TRUE(tracker_->GetActiveInstall(kExtensionId1));

  scoped_active_install.reset();
  EXPECT_FALSE(tracker_->GetActiveInstall(kExtensionId1));
}

// Verifies that ScopedActiveInstall can be cancelled.
TEST_F(InstallTrackerTest, ScopedActiveInstallCancelled) {
  ActiveInstallData install_data(kExtensionId1);
  install_data.percent_downloaded = 87;
  std::unique_ptr<ScopedActiveInstall> scoped_active_install(
      new ScopedActiveInstall(tracker(), install_data));

  const ActiveInstallData* retrieved_data =
      tracker_->GetActiveInstall(kExtensionId1);
  ASSERT_TRUE(retrieved_data);
  VerifyInstallData(install_data, *retrieved_data);
  retrieved_data = nullptr;

  scoped_active_install->CancelDeregister();
  scoped_active_install.reset();

  retrieved_data = tracker_->GetActiveInstall(kExtensionId1);
  ASSERT_TRUE(retrieved_data);
  VerifyInstallData(install_data, *retrieved_data);
}

// Verifies that the download progress is updated correctly.
TEST_F(InstallTrackerTest, DownloadProgressUpdated) {
  ActiveInstallData install_data(kExtensionId1);
  tracker_->AddActiveInstall(install_data);

  const ActiveInstallData* retrieved_data =
      tracker_->GetActiveInstall(kExtensionId1);
  ASSERT_TRUE(retrieved_data);
  EXPECT_EQ(0, retrieved_data->percent_downloaded);

  const int kUpdatedDownloadProgress = 23;
  tracker_->OnDownloadProgress(kExtensionId1, kUpdatedDownloadProgress);

  retrieved_data = tracker_->GetActiveInstall(kExtensionId1);
  ASSERT_TRUE(retrieved_data);
  EXPECT_EQ(kUpdatedDownloadProgress, retrieved_data->percent_downloaded);
}

// Verifies that OnBeginExtensionInstall() registers an active install and
// OnInstallFailure() removes an active install.
TEST_F(InstallTrackerTest, ExtensionInstallFailure) {
  InstallObserver::ExtensionInstallParams install_params(
      kExtensionId1, std::string(), gfx::ImageSkia(), false, false);
  tracker_->OnBeginExtensionInstall(install_params);

  const ActiveInstallData* retrieved_data =
      tracker_->GetActiveInstall(kExtensionId1);
  ASSERT_TRUE(retrieved_data);
  EXPECT_EQ(0, retrieved_data->percent_downloaded);
  EXPECT_EQ(install_params.extension_id, retrieved_data->extension_id);
  retrieved_data = nullptr;

  tracker_->OnInstallFailure(kExtensionId1);
  EXPECT_FALSE(tracker_->GetActiveInstall(kExtensionId1));
}

// Verifies that OnExtensionInstalled() notification removes an active install.
TEST_F(InstallTrackerTest, ExtensionInstalledEvent) {
  InstallObserver::ExtensionInstallParams install_params(
      kExtensionId1, std::string(), gfx::ImageSkia(), false, false);
  tracker_->OnBeginExtensionInstall(install_params);

  const ActiveInstallData* retrieved_data =
      tracker_->GetActiveInstall(kExtensionId1);
  ASSERT_TRUE(retrieved_data);
  EXPECT_EQ(0, retrieved_data->percent_downloaded);
  EXPECT_EQ(install_params.extension_id, retrieved_data->extension_id);
  retrieved_data = nullptr;

  // Simulate an extension install.
  scoped_refptr<const Extension> extension =
      CreateDummyExtension(kExtensionId1);
  ASSERT_TRUE(extension.get());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry);
  registry->AddEnabled(extension);
  registry->TriggerOnInstalled(extension.get(), false);

  EXPECT_FALSE(tracker_->GetActiveInstall(kExtensionId1));
}
