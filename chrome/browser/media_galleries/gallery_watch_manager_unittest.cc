// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/gallery_watch_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
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
        profile_(new TestingProfile()),
        gallery_prefs_(nullptr),
        expect_gallery_changed_(false),
        expect_gallery_watch_dropped_(false),
        pending_loop_(nullptr) {
  }

  GalleryWatchManagerTest(const GalleryWatchManagerTest&) = delete;
  GalleryWatchManagerTest& operator=(const GalleryWatchManagerTest&) = delete;

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

    manager_ = std::make_unique<GalleryWatchManager>();
    manager_->AddObserver(profile_.get(), this);
  }

  void TearDown() override {
    if (profile_) {
      manager_->RemoveObserver(profile_.get());
    }
    manager_.reset();
    monitor_ = nullptr;

    // The TestingProfile must be destroyed before the TestingBrowserProcess
    // because TestingProfile uses TestingBrowserProcess in its destructor.
    ShutdownProfile();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The UserManager must be destroyed before the TestingBrowserProcess
    // because UserManager uses TestingBrowserProcess in its destructor.
    user_manager_.Reset();
#endif

    // Make sure any pending network events are run before the
    // NetworkConnectionTracker is cleared.
    task_environment_.RunUntilIdle();

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
    manager()->AddWatch(profile(), extension(), gallery_id,
                        base::BindOnce(&ConfirmWatch, base::Unretained(&loop)));
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

  void ShutdownProfile() {
    gallery_prefs_ = nullptr;
    profile_.reset();
  }

 private:
  // GalleryWatchManagerObserver implementation.
  void OnGalleryChanged(const std::string& extension_id,
                        MediaGalleryPrefId gallery_id) override {
    EXPECT_TRUE(expect_gallery_changed_);
    pending_loop_->Quit();
    pending_loop_ = nullptr;
  }

  void OnGalleryWatchDropped(const std::string& extension_id,
                             MediaGalleryPrefId gallery_id) override {
    EXPECT_TRUE(expect_gallery_watch_dropped_);
    pending_loop_->Quit();
    pending_loop_ = nullptr;
  }

  std::unique_ptr<GalleryWatchManager> manager_;

  // Needed for extension service & friends to work.
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<extensions::Extension> extension_;

  EnsureMediaDirectoriesExists mock_gallery_locations_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif

  raw_ptr<storage_monitor::TestStorageMonitor> monitor_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MediaGalleriesPreferences> gallery_prefs_;

  bool expect_gallery_changed_;
  bool expect_gallery_watch_dropped_;
  raw_ptr<base::RunLoop> pending_loop_;
};

// TODO(crbug.com/41443722): Flaky on ChromeOS and macOS.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
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
    manager()->AddWatch(profile(), extension(), id,
                        base::BindOnce(&ConfirmWatch, base::Unretained(&loop)));
  } else {
    manager()->AddWatch(
        profile(), extension(), id,
        base::BindOnce(&ExpectWatchError, base::Unretained(&loop),
                       GalleryWatchManager::kCouldNotWatchGalleryError));
  }
  loop.Run();
}

// TODO(crbug.com/40171219): Flaky on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AddAndRemoveTwoWatches DISABLED_AddAndRemoveTwoWatches
#else
#define MAYBE_AddAndRemoveTwoWatches AddAndRemoveTwoWatches
#endif
TEST_F(GalleryWatchManagerTest, MAYBE_AddAndRemoveTwoWatches) {
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

// TODO(crbug.com/40751695): Flaky on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RemoveAllWatches DISABLED_RemoveAllWatches
#else
#define MAYBE_RemoveAllWatches RemoveAllWatches
#endif
TEST_F(GalleryWatchManagerTest, MAYBE_RemoveAllWatches) {
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

// Fails on Mac: crbug.com/1183212
// Fails on Chrome OS: crbug.com/1207878
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DropWatchOnGalleryRemoved DISABLED_DropWatchOnGalleryRemoved
#else
#define MAYBE_DropWatchOnGalleryRemoved DropWatchOnGalleryRemoved
#endif
TEST_F(GalleryWatchManagerTest, MAYBE_DropWatchOnGalleryRemoved) {
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

// TODO(crbug.com/40751910): flaky on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DropWatchOnStorageRemoved DISABLED_DropWatchOnStorageRemoved
#else
#define MAYBE_DropWatchOnStorageRemoved DropWatchOnStorageRemoved
#endif
TEST_F(GalleryWatchManagerTest, MAYBE_DropWatchOnStorageRemoved) {
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

// Test is flaky. https://crbug.com/40752685
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_TestWatchOperation DISABLED_TestWatchOperation
#else
#define MAYBE_TestWatchOperation TestWatchOperation
#endif
TEST_F(GalleryWatchManagerTest, MAYBE_TestWatchOperation) {
  if (!GalleryWatchesSupported())
    return;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  MediaGalleryPrefId id = AddGallery(temp_dir.GetPath());
  AddAndConfirmWatch(id);

  base::RunLoop success_loop;
  ExpectGalleryChanged(&success_loop);
  ASSERT_TRUE(
      base::WriteFile(temp_dir.GetPath().AppendASCII("fake file"), "blah"));
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
  ASSERT_TRUE(
      base::WriteFile(temp_dir.GetPath().AppendASCII("fake file"), "blah"));
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
