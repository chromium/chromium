// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "content/public/test/browser_test.h"

namespace drive {

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using testing::_;

using DriveIntegrationServiceBrowserTest =
    DriveIntegrationServiceBrowserTestBase;

// Verify DriveIntegrationService is created during login.
IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest, CreatedDuringLogin) {
  EXPECT_TRUE(
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       ClearCacheAndRemountFileSystem) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());
  base::FilePath cache_path = drive_service->GetDriveFsHost()->GetDataPath();
  base::FilePath log_folder_path = drive_service->GetDriveFsLogPath().DirName();
  base::FilePath cache_file;
  base::FilePath log_file;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(cache_path));
    ASSERT_TRUE(base::CreateDirectory(log_folder_path));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(cache_path, &cache_file));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(log_folder_path, &log_file));
  }

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  drive_service->ClearCacheAndRemountFileSystem(
      base::BindLambdaForTesting([=](bool success) {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(success);
        EXPECT_FALSE(base::PathExists(cache_file));
        EXPECT_TRUE(base::PathExists(log_file));
        quit_closure.Run();
      }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       DisableDrivePolicyTest) {
  // First make sure the pref is set to its default value which should permit
  // drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, false);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  EXPECT_TRUE(integration_service);
  EXPECT_TRUE(integration_service->is_enabled());

  // ...next try to disable drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);

  EXPECT_EQ(integration_service,
            drive::DriveIntegrationServiceFactory::FindForProfile(
                browser()->profile()));
  EXPECT_FALSE(integration_service->is_enabled());
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       SearchDriveByFileNameTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  base::FilePath mount_path = drive_service->GetMountPointPath();
  ASSERT_TRUE(base::WriteFile(mount_path.Append("bar"), ""));
  ASSERT_TRUE(base::WriteFile(mount_path.Append("baz"), ""));
  auto base_time = base::Time::Now() - base::Seconds(10);
  auto earlier_time = base_time - base::Seconds(10);
  ASSERT_TRUE(base::TouchFile(mount_path.Append("bar"), base_time, base_time));
  ASSERT_TRUE(
      base::TouchFile(mount_path.Append("baz"), earlier_time, earlier_time));

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  drive_service->SearchDriveByFileName(
      "ba", 10, drivefs::mojom::QueryParameters::SortField::kLastViewedByMe,
      drivefs::mojom::QueryParameters::SortDirection::kAscending,
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
      base::BindLambdaForTesting(
          [=](FileError error,
              std::vector<drivefs::mojom::QueryItemPtr> items) {
            EXPECT_EQ(2u, items.size());
            EXPECT_EQ("baz", items[0]->path.BaseName().value());
            EXPECT_EQ("bar", items[1]->path.BaseName().value());
            quit_closure.Run();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest, GetThumbnailTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  base::FilePath mount_path = drive_service->GetMountPointPath();
  ASSERT_TRUE(base::WriteFile(mount_path.Append("bar"), ""));

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  drive_service->GetThumbnail(
      base::FilePath("/bar"), true,
      base::BindLambdaForTesting(
          [=](const absl::optional<std::vector<uint8_t>>& image) {
            ASSERT_FALSE(image.has_value());
            quit_closure.Run();
          }));
  run_loop.Run();
}

class DriveIntegrationServiceWithGaiaDisabledBrowserTest
    : public DriveIntegrationServiceBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kDisableGaiaServices);
  }
};

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceWithGaiaDisabledBrowserTest,
                       DriveDisabled) {
  // First make sure the pref is set to its default value which would normally
  // permit drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, false);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  ASSERT_TRUE(integration_service);
  EXPECT_FALSE(integration_service->is_enabled());
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest, GetMetadata) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  base::FilePath mount_path = drive_service->GetMountPointPath();
  base::FilePath file_path;
  base::CreateTemporaryFileInDir(mount_path, &file_path);

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->GetMetadata(
        base::FilePath("/foo/bar"),
        base::BindLambdaForTesting(
            [=](FileError error, drivefs::mojom::FileMetadataPtr metadata_ptr) {
              EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
              quit_closure.Run();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->GetMetadata(
        file_path,
        base::BindLambdaForTesting(
            [=](FileError error, drivefs::mojom::FileMetadataPtr metadata_ptr) {
              EXPECT_EQ(FILE_ERROR_OK, error);
              quit_closure.Run();
            }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       LocateFilesByItemIds) {
  Profile* profile = browser()->profile();
  InitTestFileMountRoot(profile);
  AddDriveFileWithRelativePath(profile, /*drive_file_id=*/"abc123",
                               /*directory_path=*/base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               /*new_file_absolute_path=*/nullptr);
  base::FilePath relative_file_path;
  AddDriveFileWithRelativePath(profile, /*drive_file_id=*/"qwertyqwerty",
                               /*directory_path=*/base::FilePath("aa"),
                               &relative_file_path,
                               /*new_file_absolute_path=*/nullptr);
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    DriveIntegrationServiceFactory::FindForProfile(profile)
        ->LocateFilesByItemIds(
            {"qwertyqwerty", "foobar"},
            base::BindLambdaForTesting(
                [=](absl::optional<
                    std::vector<drivefs::mojom::FilePathOrErrorPtr>> result) {
                  ASSERT_EQ(2u, result->size());
                  EXPECT_EQ(relative_file_path, base::FilePath("/").Append(
                                                    result->at(0)->get_path()));
                  EXPECT_EQ(FILE_ERROR_NOT_FOUND, result->at(1)->get_error());
                  quit_closure.Run();
                }));
    run_loop.Run();
  }
}

class DriveIntegrationServiceWithPrefDisabledBrowserTest
    : public DriveIntegrationServiceBrowserTest {
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) override {
    profile->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);
    return DriveIntegrationServiceBrowserTest::CreateDriveIntegrationService(
        profile);
  }
};

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceWithPrefDisabledBrowserTest,
                       RenableAndDisableDrive) {
  auto* profile = browser()->profile();
  auto* drive_service = DriveIntegrationServiceFactory::FindForProfile(profile);
  EXPECT_FALSE(drive_service->is_enabled());

  profile->GetPrefs()->SetBoolean(prefs::kDisableDrive, false);
  EXPECT_TRUE(drive_service->is_enabled());

  profile->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);
  EXPECT_FALSE(drive_service->is_enabled());
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       EnableMirrorSync_FeatureDisabled) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->ToggleMirroring(
        /*enabled=*/true,
        base::BindLambdaForTesting(
            [quit_closure](drivefs::mojom::MirrorSyncStatus status) {
              EXPECT_EQ(drivefs::mojom::MirrorSyncStatus::kFeatureNotEnabled,
                        status);
              quit_closure.Run();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->ToggleMirroring(
        /*enabled=*/false,
        base::BindLambdaForTesting(
            [quit_closure](drivefs::mojom::MirrorSyncStatus status) {
              EXPECT_EQ(drivefs::mojom::MirrorSyncStatus::kFeatureNotEnabled,
                        status);
              quit_closure.Run();
            }));
    run_loop.Run();
  }
}

class DriveMirrorSyncStatusObserver : public DriveIntegrationServiceObserver {
 public:
  explicit DriveMirrorSyncStatusObserver(bool expected_status)
      : expected_status_(expected_status) {
    quit_closure_ = run_loop_.QuitClosure();
  }

  DriveMirrorSyncStatusObserver(const DriveMirrorSyncStatusObserver&) = delete;
  DriveMirrorSyncStatusObserver& operator=(
      const DriveMirrorSyncStatusObserver&) = delete;

  ~DriveMirrorSyncStatusObserver() override {}

  void WaitForStatusChange() { run_loop_.Run(); }

  void OnMirroringEnabled() override {
    quit_closure_.Run();
    EXPECT_TRUE(expected_status_);
  }

  void OnMirroringDisabled() override {
    quit_closure_.Run();
    EXPECT_FALSE(expected_status_);
  }

 private:
  base::RunLoop run_loop_;
  base::RepeatingClosure quit_closure_;
  bool expected_status_ = false;
};

class DriveIntegrationBrowserTestWithMirrorSyncEnabled
    : public DriveIntegrationServiceBrowserTest {
 public:
  DriveIntegrationBrowserTestWithMirrorSyncEnabled() {
    scoped_feature_list_.InitWithFeatures({ash::features::kDriveFsMirroring},
                                          {});
  }

  DriveIntegrationBrowserTestWithMirrorSyncEnabled(
      const DriveIntegrationBrowserTestWithMirrorSyncEnabled&) = delete;
  DriveIntegrationBrowserTestWithMirrorSyncEnabled& operator=(
      const DriveIntegrationBrowserTestWithMirrorSyncEnabled&) = delete;

  ~DriveIntegrationBrowserTestWithMirrorSyncEnabled() override {}

  void ToggleMirrorSync(bool status) {
    auto observer = std::make_unique<DriveMirrorSyncStatusObserver>(status);
    auto* drive_service =
        DriveIntegrationServiceFactory::FindForProfile(browser()->profile());
    drive_service->AddObserver(observer.get());

    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kDriveFsEnableMirrorSync, status);
    observer->WaitForStatusChange();
    EXPECT_EQ(browser()->profile()->GetPrefs()->GetBoolean(
                  prefs::kDriveFsEnableMirrorSync),
              status);

    drive_service->RemoveObserver(observer.get());
  }

  void AddSyncingPath(const base::FilePath& path) {
    drivefs::FakeDriveFs* fake_drivefs =
        GetFakeDriveFsForProfile(browser()->profile());
    std::vector<base::FilePath> return_paths{path};
    EXPECT_CALL(*fake_drivefs, GetSyncingPaths(_))
        .WillOnce(RunOnceCallback<0>(drive::FileError::FILE_ERROR_OK,
                                     std::move(return_paths)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       EnableMirrorSync) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  // Ensure the mirror syncing service is disabled.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kDriveFsEnableMirrorSync));
  EXPECT_FALSE(drive_service->IsMirroringEnabled());

  // Enable mirroring and ensure the integration service has it enabled.
  ToggleMirrorSync(true);
  EXPECT_TRUE(drive_service->IsMirroringEnabled());
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       DisableMirrorSync) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  // Ensure the mirror syncing service is disabled.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kDriveFsEnableMirrorSync));
  EXPECT_FALSE(drive_service->IsMirroringEnabled());

  // Enable mirror syncing.
  ToggleMirrorSync(true);
  EXPECT_TRUE(drive_service->IsMirroringEnabled());

  // Disable mirroring and ensure the integration service has it disabled.
  ToggleMirrorSync(false);
  EXPECT_FALSE(drive_service->IsMirroringEnabled());
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       ToggleSyncForPath_MirroringDisabled) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->ToggleSyncForPath(
        base::FilePath("/fake/path"), drivefs::mojom::MirrorPathStatus::kStart,
        base::BindLambdaForTesting([quit_closure](FileError status) {
          EXPECT_EQ(FILE_ERROR_SERVICE_UNAVAILABLE, status);
          quit_closure.Run();
        }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       ToggleSyncForPath_MirroringEnabledFileNotFound) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  // Enable mirror sync.
  ToggleMirrorSync(true);

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->ToggleSyncForPath(
        base::FilePath("/fake/path"), drivefs::mojom::MirrorPathStatus::kStart,
        base::BindLambdaForTesting([quit_closure](FileError status) {
          EXPECT_EQ(FILE_ERROR_NOT_FOUND, status);
          quit_closure.Run();
        }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       ToggleSyncForPath_MirroringEnabled) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  // Enable mirror sync.
  ToggleMirrorSync(true);

  base::ScopedTempDir temp_dir;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  }

  {
    base::FilePath sync_path = temp_dir.GetPath();
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->ToggleSyncForPath(
        sync_path, drivefs::mojom::MirrorPathStatus::kStart,
        base::BindLambdaForTesting([quit_closure](FileError status) {
          EXPECT_EQ(FILE_ERROR_OK, status);
          quit_closure.Run();
        }));
    run_loop.Run();
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir.Delete());
  }
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       GetSyncingPaths_MirroringDisabled) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->GetSyncingPaths(base::BindLambdaForTesting(
        [quit_closure](drive::FileError status,
                       const std::vector<base::FilePath>& paths) {
          EXPECT_EQ(drive::FILE_ERROR_SERVICE_UNAVAILABLE, status);
          EXPECT_EQ(0u, paths.size());
          quit_closure.Run();
        }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       GetSyncingPaths_MirroringEnabled) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  // Enable mirror sync and add |sync_path| that we expect to return from
  // |GetSyncingPaths|.
  ToggleMirrorSync(true);

  base::ScopedTempDir temp_dir;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  }

  {
    base::FilePath sync_path = temp_dir.GetPath();
    AddSyncingPath(sync_path);

    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->GetSyncingPaths(base::BindLambdaForTesting(
        [quit_closure, sync_path](drive::FileError status,
                                  const std::vector<base::FilePath>& paths) {
          EXPECT_EQ(drive::FILE_ERROR_OK, status);
          EXPECT_EQ(1u, paths.size());
          EXPECT_EQ(sync_path, paths[0]);
          quit_closure.Run();
        }));
    run_loop.Run();
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir.Delete());
  }
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationBrowserTestWithMirrorSyncEnabled,
                       MachineRootIDPersistedAndAvailable) {
  ToggleMirrorSync(true);

  // Ensure the initial machine root ID is unset.
  EXPECT_EQ(browser()->profile()->GetPrefs()->GetString(
                prefs::kDriveFsMirrorSyncMachineRootId),
            "");

  // Invoke the delegate method to persist the machine root ID and wait for the
  // prefs key to change to the expected value.
  drivefs::FakeDriveFs* fake = GetFakeDriveFsForProfile(browser()->profile());
  fake->delegate()->PersistMachineRootID("test-machine-id");
  WaitForPrefValue(browser()->profile()->GetPrefs(),
                   prefs::kDriveFsMirrorSyncMachineRootId,
                   base::Value("test-machine-id"));

  // Setup the callback for the GetMachineRootID method to assert it gets run
  // with the "test-machine-id".
  base::RunLoop run_loop;
  base::MockOnceCallback<void(const std::string&)> machine_root_id_callback;
  EXPECT_CALL(machine_root_id_callback, Run("test-machine-id"))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Kick off the GetMachineRootID method and wait for it to return
  // successfully.
  fake->delegate()->GetMachineRootID(machine_root_id_callback.Get());
  run_loop.Run();
}

}  // namespace drive
