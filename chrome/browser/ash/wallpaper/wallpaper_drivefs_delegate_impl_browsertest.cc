// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper/wallpaper_drivefs_delegate_impl.h"

#include <memory>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
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

// Saves a test wallpaper file and returns the expected metadata `modified_at`
// time.
base::Time SaveTestWallpaperFile(const AccountId& account_id) {
  const base::FilePath wallpaper_path =
      WallpaperControllerClientImpl::Get()->GetWallpaperPathFromDriveFs(
          account_id);
  DCHECK(!wallpaper_path.empty());
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::DirectoryExists(wallpaper_path.DirName())) {
    EXPECT_TRUE(base::CreateDirectory(wallpaper_path.DirName()));
  }
  const auto data = EncodeImage(CreateTestImage());
  const size_t size = base::WriteFile(
      wallpaper_path, data->front_as<const char>(), data->size());
  DCHECK_EQ(size, data->size());

  base::File::Info info;
  base::GetFileInfo(wallpaper_path, &info);
  return info.last_modified;
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

  WallpaperDriveFsDelegate* GetWallpaperDriveFsDelegate() const {
    Shell* shell = Shell::Get();
    auto* wallpaper_controller = shell->wallpaper_controller();
    return wallpaper_controller->drivefs_delegate_for_testing();
  }

  const AccountId& GetAccountId() const {
    user_manager::User* user =
        ProfileHelper::Get()->GetUserByProfile(browser()->profile());
    DCHECK(user);
    return user->GetAccountId();
  }

  base::Time GetWallpaperModificationTimeSync(
      const AccountId& account_id) const {
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
  SaveTestWallpaperFile(GetAccountId());

  drive::DriveIntegrationService* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(browser()->profile());
  DCHECK(drive_integration_service);
  drive_integration_service->SetEnabled(false);

  const base::Time modification_time =
      GetWallpaperModificationTimeSync(GetAccountId());
  EXPECT_EQ(modification_time, base::Time())
      << "DriveFS disabled should result in empty time";
}

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest,
                       RespondsWithModifiedAtTime) {
  InitTestFileMountRoot(browser()->profile());
  const base::Time expected = SaveTestWallpaperFile(GetAccountId());
  const base::Time actual = GetWallpaperModificationTimeSync(GetAccountId());
  EXPECT_EQ(actual, expected)
      << "DriveFS modified_at should match file modified time";
}

}  // namespace ash
