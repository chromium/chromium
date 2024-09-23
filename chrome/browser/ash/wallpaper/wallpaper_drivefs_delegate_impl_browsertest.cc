// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/wallpaper/wallpaper_drivefs_delegate_impl.h"

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/barrier_closure.h"
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
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

scoped_refptr<base::RefCountedBytes> EncodeImage(const gfx::ImageSkia& image) {
  auto output = base::MakeRefCounted<base::RefCountedBytes>();
  SkBitmap bitmap = *(image.bitmap());
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/90, &(output)->as_vector());
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
  auto data = EncodeImage(gfx::test::CreateImageSkia(/*size=*/16));
  ASSERT_TRUE(
      base::WriteFile(target, base::make_span(data->front(), data->size())));
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

  std::vector<drivefs::mojom::FileChangePtr> CreateWallpaperFileChange() {
    std::vector<drivefs::mojom::FileChangePtr> file_changes;
    drive::DriveIntegrationService* drive_integration_service =
        drive::util::GetIntegrationServiceByProfile(browser()->profile());

    base::FilePath fake_wallpaper_notification_path(
        &base::FilePath::kSeparators[0]);

    EXPECT_TRUE(
        drive_integration_service->GetMountPointPath().AppendRelativePath(
            GetWallpaperDriveFsDelegate()->GetWallpaperPath(GetAccountId()),
            &fake_wallpaper_notification_path));

    drivefs::mojom::FileChangePtr wallpaper_change =
        drivefs::mojom::FileChange::New(
            fake_wallpaper_notification_path,
            drivefs::mojom::FileChange::Type::kModify);

    file_changes.push_back(std::move(wallpaper_change));
    return file_changes;
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

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest,
                       WaitForWallpaperChange) {
  InitTestFileMountRoot(browser()->profile());

  base::RunLoop loop;

  GetWallpaperDriveFsDelegate()->WaitForWallpaperChange(
      GetAccountId(), base::BindLambdaForTesting([&loop](bool success) {
        EXPECT_TRUE(success);
        loop.Quit();
      }));

  // Send the fake wallpaper file change notification.
  drivefs::FakeDriveFs* fake_drivefs =
      GetFakeDriveFsForProfile(browser()->profile());
  fake_drivefs->delegate()->OnFilesChanged(CreateWallpaperFileChange());

  loop.Run();
}

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest,
                       WaitForWallpaperChangeWithDriveFsDisabled) {
  drive::DriveIntegrationService* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(browser()->profile());
  drive_integration_service->SetEnabled(false);

  base::RunLoop loop;

  // Responds immediately with `success=false` even without receiving any file
  // change notifications because DriveFS is disabled.
  GetWallpaperDriveFsDelegate()->WaitForWallpaperChange(
      GetAccountId(), base::BindLambdaForTesting([&loop](bool success) {
        EXPECT_FALSE(success);
        loop.Quit();
      }));

  loop.Run();
}

IN_PROC_BROWSER_TEST_F(WallpaperDriveFsDelegateImplBrowserTest,
                       WaitForWallpaperChangeMultipleTimes) {
  InitTestFileMountRoot(browser()->profile());

  base::RunLoop loop;
  // Make sure that closure is called twice before `loop` quits.
  base::RepeatingClosure barrier =
      base::BarrierClosure(/*num_closures=*/2, loop.QuitClosure());

  // The first `WaitForWallpaperChange` call should respond with false when
  // `WaitForWallpaperChange` is called a second time before receiving a file
  // change notification.
  GetWallpaperDriveFsDelegate()->WaitForWallpaperChange(
      GetAccountId(), base::BindLambdaForTesting([&barrier](bool success) {
        EXPECT_FALSE(success);
        barrier.Run();
      }));

  // This call to `WaitForWallpaperChange` should succeed upon receiving the
  // file change notification.
  GetWallpaperDriveFsDelegate()->WaitForWallpaperChange(
      GetAccountId(), base::BindLambdaForTesting([&barrier](bool success) {
        EXPECT_TRUE(success);
        barrier.Run();
      }));

  // Send the fake wallpaper file change notification.
  drivefs::FakeDriveFs* fake_drivefs =
      GetFakeDriveFsForProfile(browser()->profile());
  fake_drivefs->delegate()->OnFilesChanged(CreateWallpaperFileChange());

  loop.Run();
}

}  // namespace ash
