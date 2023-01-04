// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper/wallpaper_drivefs_delegate_impl.h"

#include <memory>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

gfx::ImageSkia CreateTestImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorGREEN);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

scoped_refptr<base::RefCountedBytes> EncodeImage(const gfx::ImageSkia& image) {
  auto output = base::MakeRefCounted<base::RefCountedBytes>();
  SkBitmap bitmap = *(image.bitmap());
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/90, &(output)->data());
  return output;
}

WallpaperDriveFsDelegate* GetWallpaperDriveFsDelegate() {
  Shell* shell = Shell::Get();
  auto* wallpaper_controller = shell->wallpaper_controller();
  return wallpaper_controller->drivefs_delegate_for_testing();
}

// Saves a test wallpaper file. If `target` is empty, will default to the
// DriveFS wallpaper path.
void SaveTestWallpaperFile(const AccountId& account_id, base::FilePath target) {
  ASSERT_FALSE(target.empty()) << "target FilePath is required to be non-empty";
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::DirectoryExists(target.DirName())) {
    ASSERT_TRUE(base::CreateDirectory(target.DirName()));
  }
  auto data = EncodeImage(CreateTestImage());
  size_t size =
      base::WriteFile(target, data->front_as<const char>(), data->size());
  ASSERT_EQ(size, data->size());
}

}  // namespace

class WallpaperDriveFsDelegateImplBrowserTest
    : public drive::DriveIntegrationServiceBrowserTestBase {
 public:
  WallpaperDriveFsDelegateImplBrowserTest() = default;

  WallpaperDriveFsDelegateImplBrowserTest(
      const WallpaperDriveFsDelegateImplBrowserTest&) = delete;
  WallpaperDriveFsDelegateImplBrowserTest& operator=(
      const WallpaperDriveFsDelegateImplBrowserTest&) = delete;

  ~WallpaperDriveFsDelegateImplBrowserTest() override = default;

  const AccountId& GetAccountId() const {
    user_manager::User* user =
        ProfileHelper::Get()->GetUserByProfile(browser()->profile());
    DCHECK(user);
    return user->GetAccountId();
  }

  bool SaveWallpaperSync(const AccountId& account_id,
                         const base::FilePath& source) {
    base::RunLoop loop;
    bool out = false;
    GetWallpaperDriveFsDelegate()->SaveWallpaper(
        account_id, source,
        base::BindLambdaForTesting([&out, &loop](bool success) {
          out = success;
          loop.Quit();
        }));
    loop.Run();
    return out;
  }

  base::Time GetWallpaperModificationTimeSync(const AccountId& account_id) {
    base::RunLoop loop;
    base::Time out;
    GetWallpaperDriveFsDelegate()->GetWallpaperModificationTime(
        account_id, base::BindLambdaForTesting([&out, &loop](base::Time time) {
          out = time;
          loop.Quit();
        }));
    loop.Run();
    return out;
  }
};

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest,
                       EmptyBaseTimeIfNoDriveFs) {
  InitTestFileMountRoot(browser()->profile());
  SaveTestWallpaperFile(
      GetAccountId(),
      GetWallpaperDriveFsDelegate()->GetWallpaperPath(GetAccountId()));

  drive::DriveIntegrationService* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(browser()->profile());
  ASSERT_TRUE(drive_integration_service);
  drive_integration_service->SetEnabled(false);

  const base::Time modification_time =
      GetWallpaperModificationTimeSync(GetAccountId());
  EXPECT_EQ(modification_time, base::Time())
      << "DriveFS disabled should result in empty time";
}

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest,
                       RespondsWithModifiedAtTime) {
  InitTestFileMountRoot(browser()->profile());

  base::ScopedAllowBlockingForTesting allow_blocking;
  const base::FilePath drivefs_wallpaper_path =
      GetWallpaperDriveFsDelegate()->GetWallpaperPath(GetAccountId());
  ASSERT_FALSE(base::PathExists(drivefs_wallpaper_path));

  SaveTestWallpaperFile(GetAccountId(), drivefs_wallpaper_path);

  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(drivefs_wallpaper_path, &file_info));

  const base::Time drivefs_modification_time =
      GetWallpaperModificationTimeSync(GetAccountId());

  EXPECT_EQ(drivefs_modification_time, file_info.last_modified)
      << "modification_time matches file info time";
}

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest, SaveWallpaper) {
  InitTestFileMountRoot(browser()->profile());

  base::FilePath drivefs_wallpaper_path =
      GetWallpaperDriveFsDelegate()->GetWallpaperPath(GetAccountId());
  ASSERT_FALSE(drivefs_wallpaper_path.empty());

  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  // Write a jpg file to a tmp directory. This file will be copied into DriveFS.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath source_jpg = scoped_temp_dir.GetPath().Append("source.jpg");
  SaveTestWallpaperFile(GetAccountId(), source_jpg);

  // `SaveWallpaper` should succeed while DriveFS is enabled.
  EXPECT_TRUE(SaveWallpaperSync(GetAccountId(), source_jpg));
  // source.jpg was copied to DriveFS wallpaper path.
  EXPECT_TRUE(base::PathExists(drivefs_wallpaper_path));
}

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest,
                       SaveWallpaperDriveFsDisabled) {
  InitTestFileMountRoot(browser()->profile());
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  // Write a jpg file to a tmp directory. This file will be copied into DriveFS.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath source_jpg = scoped_temp_dir.GetPath().Append("source.jpg");
  SaveTestWallpaperFile(GetAccountId(), source_jpg);

  base::FilePath drivefs_wallpaper_path =
      GetWallpaperDriveFsDelegate()->GetWallpaperPath(GetAccountId());
  ASSERT_FALSE(drivefs_wallpaper_path.empty());

  // Call `SaveWallpaper` while DriveFS is disabled. No file should be written.
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(browser()->profile());
  drive_integration_service->SetEnabled(false);
  EXPECT_FALSE(SaveWallpaperSync(GetAccountId(), source_jpg));
  EXPECT_FALSE(base::PathExists(drivefs_wallpaper_path));
}

}  // namespace ash
