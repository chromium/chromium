// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/gallery_watch_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/gallery_watch_manager_observer.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

namespace component_updater {

namespace {

void ConfirmWatch(base::RunLoop* loop, const std::string& error) {
  EXPECT_TRUE(error.empty());
  loop->Quit();
}

void ExpectWatchError(base::RunLoop* loop,
                      const std::string& expected_error,
                      const std::string& error) {
  EXPECT_EQ(expected_error, error);
  loop->Quit();
}

}  // namespace

class GalleryWatchManagerTest : public GalleryWatchManagerObserver,
                                public testing::Test {
 public:
  GalleryWatchManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
#if defined(OS_CHROMEOS)
        test_user_manager_(std::make_unique<chromeos::ScopedTestUserManager>()),
#endif
        profile_(new TestingProfile()),
        gallery_prefs_(NULL),
        expect_gallery_changed_(false),
        expect_gallery_watch_dropped_(false),
        pending_loop_(NULL) {
  }

  ~GalleryWatchManagerTest() override {}

  void SetUp() override {
    monitor_ = storage_monitor::TestStorageMonitor::CreateAndInstall();
    ASSERT_TRUE(monitor_);

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

    gallery_prefs_ =
        MediaGalleriesPreferencesFactory::GetForProfile(profile_.get());
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        chrome_apps::MediaGalleriesPermission::kReadPermission);
    extension_ = AddMediaGalleriesApp("read", read_permissions, profile_.get());

    manager_.reset(new GalleryWatchManager);
    manager_->AddObserver(profile_.get(), this);
  }

  void TearDown() override {
    if (profile_) {
      manager_->RemoveObserver(profile_.get());
    }
    manager_.reset();

    // The TestingProfile must be destroyed before the TestingBrowserProcess
    // because TestingProfile uses TestingBrowserProcess in its destructor.
    ShutdownProfile();

#if defined(OS_CHROMEOS)
    // The TestUserManager must be destroyed before the TestingBrowserProcess
    // because TestUserManager uses TestingBrowserProcess in its destructor.
    test_user_manager_.reset();
#endif

    // The MediaFileSystemRegistry owned by the TestingBrowserProcess must be
    // destroyed before the StorageMonitor because it calls
    // StorageMonitor::RemoveObserver() in its destructor.
    TestingBrowserProcess::DeleteInstance();

    storage_monitor::TestStorageMonitor::Destroy();
  }

 protected:
  // Create the specified path, and add it to preferences as a gallery.
  MediaGalleryPrefId AddGallery(const base::FilePath& path) {
    MediaGalleryPrefInfo gallery_info;
    EXPECT_FALSE(gallery_prefs_->LookUpGalleryByPath(path, &gallery_info));
    MediaGalleryPrefId id =
        gallery_prefs_->AddGallery(gallery_info.device_id,
                                   gallery_info.path,
                                   MediaGalleryPrefInfo::kUserAdded,
                                   gallery_info.volume_label,
                                   gallery_info.vendor_name,
                                   gallery_info.model_name,
                                   gallery_info.total_size_in_bytes,
                                   gallery_info.last_attach_time,
                                   0,
                                   0,
                                   0);
    EXPECT_NE(kInvalidMediaGalleryPrefId, id);

    EXPECT_TRUE(gallery_prefs_->SetGalleryPermissionForExtension(
        *extension_, id, true));
    return id;
  }

  TestingProfile* profile() { return profile_.get(); }

  GalleryWatchManager* manager() { return manager_.get(); }

  extensions::Extension* extension() { return extension_.get(); }

  MediaGalleriesPreferences* gallery_prefs() { return gallery_prefs_; }

  storage_monitor::TestStorageMonitor* storage_monitor() { return monitor_; }

  bool GalleryWatchesSupported() {
    return base::FilePathWatcher::RecursiveWatchAvailable();
  }

  void AddAndConfirmWatch(MediaGalleryPrefId gallery_id) {
    base::RunLoop loop;
    manager()->AddWatch(profile(),
                        extension(),
                        gallery_id,
                        base::Bind(&ConfirmWatch, base::Unretained(&loop)));
    loop.Run();
  }

  void ExpectGalleryChanged(base::RunLoop* loop) {
    expect_gallery_changed_ = true;
    pending_loop_ = loop;
  }

  void ExpectGalleryWatchDropped(base::RunLoop* loop) {
    expect_gallery_watch_dropped_ = true;
    pending_loop_ = loop;
  }

  void ShutdownProfile() { profile_.reset(nullptr); }

 private:
  // GalleryWatchManagerObserver implementation.
  void OnGalleryChanged(const std::string& extension_id,
                        MediaGalleryPrefId gallery_id) override {
    EXPECT_TRUE(expect_gallery_changed_);
    pending_loop_->Quit();
  }

  void OnGalleryWatchDropped(const std::string& extension_id,
                             MediaGalleryPrefId gallery_id) override {
    EXPECT_TRUE(expect_gallery_watch_dropped_);
    pending_loop_->Quit();
  }

  std::unique_ptr<GalleryWatchManager> manager_;

  // Needed for extension service & friends to work.
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<extensions::Extension> extension_;

  EnsureMediaDirectoriesExists mock_gallery_locations_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  std::unique_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

  storage_monitor::TestStorageMonitor* monitor_;
  std::unique_ptr<TestingProfile> profile_;
  MediaGalleriesPreferences* gallery_prefs_;

  bool expect_gallery_changed_;
  bool expect_gallery_watch_dropped_;
  base::RunLoop* pending_loop_;

  DISALLOW_COPY_AND_ASSIGN(GalleryWatchManagerTest);
};

// TODO(crbug.com/936065): Flaky on ChromeOS.
#if defined(OS_CHROMEOS)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST_F(GalleryWatchManagerTest, MAYBE_Basic) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());

  base::RunLoop loop;
  if (GalleryWatchesSupported()) {
    manager()->AddWatch(profile(),
                        extension(),
                        id,
                        base::Bind(&ConfirmWatch, base::Unretained(&loop)));
  } else {
    manager()->AddWatch(
        profile(),
        extension(),
        id,
        base::Bind(&ExpectWatchError,
                   base::Unretained(&loop),
                   GalleryWatchManager::kCouldNotWatchGalleryError));
  }
  loop.Run();
}

TEST_F(GalleryWatchManagerTest, AddAndRemoveTwoWatches) {
  if (!GalleryWatchesSupported())
    return;

  EXPECT_TRUE(manager()->GetWatchSet(profile(), extension()->id()).empty());

  base::ScopedTempDir temp1;
  ASSERT_TRUE(temp1.CreateUniqueTempDir());
  MediaGalleryPrefId id1 = AddGallery(temp1.GetPath());

  base::ScopedTempDir temp2;
  ASSERT_TRUE(temp2.CreateUniqueTempDir());
  MediaGalleryPrefId id2 = AddGallery(temp2.GetPath());

  // Add first watch and test it was added correctly.
  AddAndConfirmWatch(id1);
  MediaGalleryPrefIdSet set1 =
      manager()->GetWatchSet(profile(), extension()->id());
  EXPECT_EQ(1u, set1.size());
  EXPECT_TRUE(base::Contains(set1, id1));

  // Test that the second watch was added correctly too.
  AddAndConfirmWatch(id2);
  MediaGalleryPrefIdSet set2 =
      manager()->GetWatchSet(profile(), extension()->id());
  EXPECT_EQ(2u, set2.size());
  EXPECT_TRUE(base::Contains(set2, id1));
  EXPECT_TRUE(base::Contains(set2, id2));

  // Remove first watch and test that the second is still in there.
  manager()->RemoveWatch(profile(), extension()->id(), id1);
  MediaGalleryPrefIdSet set3 =
      manager()->GetWatchSet(profile(), extension()->id());
  EXPECT_EQ(1u, set3.size());
  EXPECT_TRUE(base::Contains(set3, id2));

  // Try removing the first watch again and test that it has no effect.
  manager()->RemoveWatch(profile(), extension()->id(), id1);
  EXPECT_EQ(1u, manager()->GetWatchSet(profile(), extension()->id()).size());

  // Remove the second watch and test that the new watch set is empty.
  manager()->RemoveWatch(profile(), extension()->id(), id2);
  EXPECT_TRUE(manager()->GetWatchSet(profile(), extension()->id()).empty());
}

TEST_F(GalleryWatchManagerTest, RemoveAllWatches) {
  if (!GalleryWatchesSupported())
    return;

  base::ScopedTempDir temp1;
  ASSERT_TRUE(temp1.CreateUniqueTempDir());
  MediaGalleryPrefId id1 = AddGallery(temp1.GetPath());

  base::ScopedTempDir temp2;
  ASSERT_TRUE(temp2.CreateUniqueTempDir());
  MediaGalleryPrefId id2 = AddGallery(temp2.GetPath());

  // Add watches.
  AddAndConfirmWatch(id1);
  AddAndConfirmWatch(id2);

  EXPECT_EQ(2u, manager()->GetWatchSet(profile(), extension()->id()).size());

  // RemoveAllWatches using a different extension ID and verify watches remain.
  manager()->RemoveAllWatches(profile(), "OtherExtensionId");
  EXPECT_EQ(2u, manager()->GetWatchSet(profile(), extension()->id()).size());

  // RemoveAllWatches using the correct extension ID and verify watches gone.

  manager()->RemoveAllWatches(profile(), extension()->id());
  EXPECT_TRUE(manager()->GetWatchSet(profile(), extension()->id()).empty());
}

TEST_F(GalleryWatchManagerTest, DropWatchOnGalleryRemoved) {
  if (!GalleryWatchesSupported())
    return;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());
  AddAndConfirmWatch(id);

  base::RunLoop success_loop;
  ExpectGalleryWatchDropped(&success_loop);
  gallery_prefs()->EraseGalleryById(id);
  success_loop.Run();
}

TEST_F(GalleryWatchManagerTest, DropWatchOnGalleryPermissionRevoked) {
  if (!GalleryWatchesSupported())
    return;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());
  AddAndConfirmWatch(id);

  base::RunLoop success_loop;
  ExpectGalleryWatchDropped(&success_loop);
  gallery_prefs()->SetGalleryPermissionForExtension(*extension(), id, false);
  success_loop.Run();
}

TEST_F(GalleryWatchManagerTest, DropWatchOnStorageRemoved) {
  if (!GalleryWatchesSupported())
    return;

  // Create a temporary directory and treat is as a removable storage device.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  storage_monitor()->AddRemovablePath(temp_dir.GetPath());
  storage_monitor::StorageInfo storage_info;
  ASSERT_TRUE(storage_monitor()->GetStorageInfoForPath(temp_dir.GetPath(),
                                                       &storage_info));
  storage_monitor()->receiver()->ProcessAttach(storage_info);

  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());
  AddAndConfirmWatch(id);

  base::RunLoop success_loop;
  ExpectGalleryWatchDropped(&success_loop);
  storage_monitor()->receiver()->ProcessDetach(storage_info.device_id());
  success_loop.Run();
}

TEST_F(GalleryWatchManagerTest, TestWatchOperation) {
  if (!GalleryWatchesSupported())
    return;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());
  AddAndConfirmWatch(id);

  base::RunLoop success_loop;
  ExpectGalleryChanged(&success_loop);
  ASSERT_EQ(4, base::WriteFile(temp_dir.GetPath().AppendASCII("fake file"),
                               "blah", 4));
  success_loop.Run();
}

TEST_F(GalleryWatchManagerTest, TestWatchOperationAfterProfileShutdown) {
  if (!GalleryWatchesSupported())
    return;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());
  AddAndConfirmWatch(id);

  ShutdownProfile();

  // Trigger a watch that should have been removed when the profile was
  // destroyed to catch regressions. crbug.com/467627
  base::RunLoop run_loop;
  ASSERT_EQ(4, base::WriteFile(temp_dir.GetPath().AppendASCII("fake file"),
                               "blah", 4));
  run_loop.RunUntilIdle();
}

TEST_F(GalleryWatchManagerTest, TestStorageRemovedAfterProfileShutdown) {
  if (!GalleryWatchesSupported())
    return;

  // Create a temporary directory and treat is as a removable storage device.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  storage_monitor()->AddRemovablePath(temp_dir.GetPath());
  storage_monitor::StorageInfo storage_info;
  ASSERT_TRUE(storage_monitor()->GetStorageInfoForPath(temp_dir.GetPath(),
                                                       &storage_info));
  storage_monitor()->receiver()->ProcessAttach(storage_info);

  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());
  AddAndConfirmWatch(id);

  ShutdownProfile();

  // Trigger a removable storage event that should be ignored now that the
  // profile has been destroyed to catch regressions. crbug.com/467627
  base::RunLoop run_loop;
  storage_monitor()->receiver()->ProcessDetach(storage_info.device_id());
  run_loop.RunUntilIdle();
}

}  // namespace component_updater
