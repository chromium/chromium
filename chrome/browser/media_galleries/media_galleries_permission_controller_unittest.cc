// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_permission_controller.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_test_util.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

using storage_monitor::StorageInfo;
using storage_monitor::TestStorageMonitor;

namespace {

std::string GalleryName(const MediaGalleryPrefInfo& gallery) {
  std::u16string name = gallery.GetGalleryDisplayName();
  return base::UTF16ToASCII(name);
}

}  // namespace

class MediaGalleriesPermissionControllerTest : public ::testing::Test {
 public:
  MediaGalleriesPermissionControllerTest()
      : dialog_(nullptr),
        dialog_update_count_at_destruction_(0),
        controller_(nullptr),
        profile_(new TestingProfile()) {}

  MediaGalleriesPermissionControllerTest(
      const MediaGalleriesPermissionControllerTest&) = delete;
  MediaGalleriesPermissionControllerTest& operator=(
      const MediaGalleriesPermissionControllerTest&) = delete;

  ~MediaGalleriesPermissionControllerTest() override {
    EXPECT_FALSE(controller_);
    EXPECT_FALSE(dialog_);
  }

  void SetUp() override {
    ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

    gallery_prefs_ =
        std::make_unique<MediaGalleriesPreferences>(profile_.get());
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        chrome_apps::MediaGalleriesPermission::kReadPermission);
    extension_ = AddMediaGalleriesApp("read", read_permissions, profile_.get());
  }

  void TearDown() override { TestStorageMonitor::Destroy(); }

  void StartDialog() {
    ASSERT_FALSE(controller_);
    controller_ = new MediaGalleriesPermissionController(
        *extension_.get(), gallery_prefs_.get(),
        base::BindOnce(
            &MediaGalleriesPermissionControllerTest::CreateMockDialog,
            base::Unretained(this)),
        base::BindOnce(
            &MediaGalleriesPermissionControllerTest::OnControllerDone,
            base::Unretained(this)));
  }

  MediaGalleriesPermissionController* controller() {
    return controller_;
  }

  MockMediaGalleriesDialog* dialog() {
    return dialog_;
  }

  int dialog_update_count_at_destruction() {
    EXPECT_FALSE(dialog_);
    return dialog_update_count_at_destruction_;
  }

  extensions::Extension* extension() {
    return extension_.get();
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_.get();
  }

  GalleryDialogId GetDialogIdFromPrefId(MediaGalleryPrefId pref_id);

  void TestForgottenType(MediaGalleryPrefInfo::Type type,
                         bool forget_preserves_pref_id);

 protected:
  EnsureMediaDirectoriesExists mock_gallery_locations_;

 private:
  MediaGalleriesDialog* CreateMockDialog(
      MediaGalleriesDialogController* controller) {
    EXPECT_FALSE(dialog_);
    dialog_update_count_at_destruction_ = 0;
    dialog_ = new MockMediaGalleriesDialog(base::BindOnce(
        &MediaGalleriesPermissionControllerTest::OnDialogDestroyed,
        weak_factory_.GetWeakPtr()));
    return dialog_;
  }

  void OnDialogDestroyed(int update_count) {
    EXPECT_TRUE(dialog_);
    dialog_update_count_at_destruction_ = update_count;
    dialog_ = nullptr;
  }

  void OnControllerDone() { controller_ = nullptr; }

  // Needed for extension service & friends to work.
  content::BrowserTaskEnvironment task_environment_;

  // The dialog is owned by the controller, but this pointer should only be
  // valid while the dialog is live within the controller.
  raw_ptr<MockMediaGalleriesDialog> dialog_;
  int dialog_update_count_at_destruction_;

  // The controller owns itself.
  raw_ptr<MediaGalleriesPermissionController> controller_;

  scoped_refptr<extensions::Extension> extension_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif

  TestStorageMonitor monitor_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MediaGalleriesPreferences> gallery_prefs_;

  base::WeakPtrFactory<MediaGalleriesPermissionControllerTest> weak_factory_{
      this};
};

GalleryDialogId
MediaGalleriesPermissionControllerTest::GetDialogIdFromPrefId(
    MediaGalleryPrefId pref_id) {
  return controller_->GetDialogId(pref_id);
}

void MediaGalleriesPermissionControllerTest::TestForgottenType(
    MediaGalleryPrefInfo::Type type, bool forget_preserves_pref_id) {
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  MediaGalleryPrefId forgotten1 = gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("forgotten1"), type);
  MediaGalleryPrefId forgotten2 = gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("forgotten2"), type);
  // Show dialog and accept to verify 2 entries
  StartDialog();
  EXPECT_EQ(0U, controller()->GetSectionEntries(0).size());
  EXPECT_EQ(mock_gallery_locations_.num_galleries() + 2U,
            controller()->GetSectionEntries(1).size());
  controller()->DidToggleEntry(GetDialogIdFromPrefId(forgotten1), true);
  controller()->DidToggleEntry(GetDialogIdFromPrefId(forgotten2), true);
  controller()->DialogFinished(true);
  EXPECT_EQ(2U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Forget one and cancel to see that it's still there.
  StartDialog();
  EXPECT_EQ(2U, controller()->GetSectionEntries(0).size());
  controller()->DidForgetEntry(GetDialogIdFromPrefId(forgotten1));
  EXPECT_EQ(1U, controller()->GetSectionEntries(0).size());
  controller()->DialogFinished(false);
  EXPECT_EQ(2U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Forget one and confirm to see that it's gone.
  StartDialog();
  EXPECT_EQ(2U, controller()->GetSectionEntries(0).size());
  controller()->DidForgetEntry(GetDialogIdFromPrefId(forgotten1));
  EXPECT_EQ(1U, controller()->GetSectionEntries(0).size());
  controller()->DialogFinished(true);
  EXPECT_EQ(1U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Add back and test whether the same pref id is preserved.
  StartDialog();
  ui::SelectedFileInfo file(MakeMediaGalleriesTestingPath("forgotten1"),
                            MakeMediaGalleriesTestingPath("forgotten1"));
  controller()->FileSelected(file, 0);
  controller()->DialogFinished(true);
  EXPECT_EQ(2U, gallery_prefs()->GalleriesForExtension(*extension()).size());
  MediaGalleryPrefInfo retrieved_info;
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("forgotten1"), &retrieved_info));
  EXPECT_EQ(forget_preserves_pref_id, retrieved_info.pref_id == forgotten1);

  // Add a new one and forget it & see that it's gone.
  MediaGalleryPrefId forgotten3 = gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("forgotten3"), type);
  StartDialog();
  EXPECT_EQ(2U, controller()->GetSectionEntries(0).size());
  EXPECT_EQ(mock_gallery_locations_.num_galleries() + 1U,
            controller()->GetSectionEntries(1).size());
  controller()->DidToggleEntry(GetDialogIdFromPrefId(forgotten3), true);
  controller()->DidForgetEntry(GetDialogIdFromPrefId(forgotten3));
  EXPECT_EQ(2U, controller()->GetSectionEntries(0).size());
  EXPECT_EQ(static_cast<unsigned long>(mock_gallery_locations_.num_galleries()),
            controller()->GetSectionEntries(1).size());
  controller()->DialogFinished(true);
  EXPECT_EQ(2U, gallery_prefs()->GalleriesForExtension(*extension()).size());
}

TEST_F(MediaGalleriesPermissionControllerTest, TestForgottenUserAdded) {
  TestForgottenType(MediaGalleryPrefInfo::kUserAdded,
                    false /* forget_preserves_pref_id */);
}

TEST_F(MediaGalleriesPermissionControllerTest, TestForgottenAutoDetected) {
  TestForgottenType(MediaGalleryPrefInfo::kAutoDetected,
                    true /* forget_preserves_pref_id */);
}

TEST_F(MediaGalleriesPermissionControllerTest, TestForgottenScanResult) {
  TestForgottenType(MediaGalleryPrefInfo::kScanResult,
                    true /* forget_preserves_pref_id */);
}

TEST_F(MediaGalleriesPermissionControllerTest, TestNameGeneration) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = 1;
  gallery.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::FIXED_MASS_STORAGE, "/path/to/gallery");
  gallery.type = MediaGalleryPrefInfo::kAutoDetected;
  std::string galleryName("/path/to/gallery");
#if BUILDFLAG(IS_CHROMEOS_ASH)
  galleryName = "gallery";
#endif
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.display_name = u"override";
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.display_name = std::u16string();
  gallery.volume_label = u"label";
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.path = base::FilePath(FILE_PATH_LITERAL("sub/gallery2"));
  galleryName = "/path/to/gallery/sub/gallery2";
#if BUILDFLAG(IS_CHROMEOS_ASH)
  galleryName = "gallery2";
#endif
#if BUILDFLAG(IS_WIN)
  galleryName = base::FilePath(FILE_PATH_LITERAL("/path/to/gallery"))
                    .Append(gallery.path).MaybeAsASCII();
#endif
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.path = base::FilePath();
  gallery.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      "/path/to/dcim");
  gallery.display_name = u"override";
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.volume_label = u"volume";
  gallery.vendor_name = u"vendor";
  gallery.model_name = u"model";
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.display_name = std::u16string();
  EXPECT_EQ("volume", GalleryName(gallery));

  gallery.volume_label = std::u16string();
  EXPECT_EQ("vendor, model", GalleryName(gallery));

  gallery.total_size_in_bytes = 1000000;
  EXPECT_EQ("977 KB vendor, model", GalleryName(gallery));

  gallery.path = base::FilePath(FILE_PATH_LITERAL("sub/path"));
  EXPECT_EQ("path - 977 KB vendor, model", GalleryName(gallery));
}
