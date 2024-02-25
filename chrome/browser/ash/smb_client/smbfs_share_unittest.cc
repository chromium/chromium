// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smbfs_share.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/smb_url.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/disks/mount_point.h"
#include "chromeos/ash/components/smbfs/smbfs_host.h"
#include "chromeos/ash/components/smbfs/smbfs_mounter.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::Property;
using testing::Unused;

namespace ash::smb_client {
namespace {

constexpr char kSharePath[] = "smb://share/path";
constexpr char kSharePath2[] = "smb://share/path2";
constexpr char kShareUsername[] = "user";
constexpr char kShareUsername2[] = "user2";
constexpr char kShareWorkgroup[] = "workgroup";
constexpr char kKerberosIdentity[] = "my-kerberos-identity";
constexpr char kDisplayName[] = "Public";
constexpr char kMountPath[] = "/share/mount/path";
constexpr char kFileName[] = "file_name.ext";
constexpr char kMountIdHashSeparator[] = "#";

// Creates a new VolumeManager for tests.
// By default, VolumeManager KeyedService is null for testing.
std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */,
      disks::DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

class MockVolumeManagerObsever : public file_manager::VolumeManagerObserver {
 public:
  MOCK_METHOD(void,
              OnVolumeMounted,
              (MountError error_code, const file_manager::Volume& volume),
              (override));
  MOCK_METHOD(void,
              OnVolumeUnmounted,
              (MountError error_code, const file_manager::Volume& volume),
              (override));
};

class MockSmbFsMounter : public smbfs::SmbFsMounter {
 public:
  MOCK_METHOD(void,
              Mount,
              (smbfs::SmbFsMounter::DoneCallback callback),
              (override));
};

class TestSmbFsImpl : public smbfs::mojom::SmbFs {
 public:
  MOCK_METHOD(void,
              RemoveSavedCredentials,
              (RemoveSavedCredentialsCallback),
              (override));

  MOCK_METHOD(void,
              DeleteRecursively,
              (const base::FilePath&, DeleteRecursivelyCallback),
              (override));
};

}  // namespace

class SmbFsShareTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    disks::DiskMountManager::InitializeForTesting(disk_mount_manager());
  }

  void SetUp() override {
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildVolumeManager));

    file_manager::VolumeManager::Get(&profile_)->AddObserver(&observer_);

    mounter_creation_callback_ = base::BindLambdaForTesting(
        [this](const std::string& share_path, const std::string& mount_dir_name,
               const SmbFsShare::MountOptions& options,
               smbfs::SmbFsHost::Delegate* delegate)
            -> std::unique_ptr<smbfs::SmbFsMounter> {
          EXPECT_EQ(share_path, kSharePath);
          return std::move(mounter_);
        });
  }

  void TearDown() override {
    file_manager::VolumeManager::Get(&profile_)->RemoveObserver(&observer_);
  }

  static disks::FakeDiskMountManager* disk_mount_manager() {
    static disks::FakeDiskMountManager* manager =
        new disks::FakeDiskMountManager();
    return manager;
  }

  std::unique_ptr<smbfs::SmbFsHost> CreateSmbFsHost(
      SmbFsShare* share,
      mojo::Receiver<smbfs::mojom::SmbFs>* smbfs_receiver,
      mojo::Remote<smbfs::mojom::SmbFsDelegate>* delegate) {
    return std::make_unique<smbfs::SmbFsHost>(
        std::make_unique<disks::MountPoint>(base::FilePath(kMountPath),
                                            disk_mount_manager()),
        share,
        mojo::Remote<smbfs::mojom::SmbFs>(
            smbfs_receiver->BindNewPipeAndPassRemote()),
        delegate->BindNewPipeAndPassReceiver());
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  MockVolumeManagerObsever observer_;

  SmbFsShare::MounterCreationCallback mounter_creation_callback_;
  std::unique_ptr<MockSmbFsMounter> mounter_ =
      std::make_unique<MockSmbFsMounter>();
  raw_ptr<MockSmbFsMounter, DanglingUntriaged> raw_mounter_ = mounter_.get();
};

TEST_F(SmbFsShareTest, Mount) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(
      observer_,
      OnVolumeMounted(
          MountError::kSuccess,
          AllOf(Property(&file_manager::Volume::type,
                         file_manager::VOLUME_TYPE_SMB),
                Property(&file_manager::Volume::mount_path,
                         base::FilePath(kMountPath)),
                Property(&file_manager::Volume::volume_label, kDisplayName))))
      .Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(
                             MountError::kSuccess,
                             AllOf(Property(&file_manager::Volume::type,
                                            file_manager::VOLUME_TYPE_SMB),
                                   Property(&file_manager::Volume::mount_path,
                                            base::FilePath(kMountPath)))))
      .Times(1);

  base::RunLoop run_loop;
  share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
    EXPECT_EQ(result, SmbMountResult::kSuccess);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_TRUE(share.IsMounted());
  EXPECT_EQ(share.share_url().ToString(), kSharePath);
  EXPECT_EQ(share.mount_path(), base::FilePath(kMountPath));

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath virtual_path;
  EXPECT_TRUE(mount_points->GetVirtualPath(
      base::FilePath(kMountPath).Append(kFileName), &virtual_path));
}

TEST_F(SmbFsShareTest, MountFailure) {
  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(smbfs::mojom::MountError::kTimeout, nullptr);
      });
  EXPECT_CALL(observer_, OnVolumeMounted(MountError::kSuccess, _)).Times(0);
  EXPECT_CALL(observer_, OnVolumeUnmounted(MountError::kSuccess, _)).Times(0);

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  base::RunLoop run_loop;
  share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
    EXPECT_EQ(result, SmbMountResult::kAborted);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_FALSE(share.IsMounted());
  EXPECT_EQ(share.share_url().ToString(), kSharePath);
  EXPECT_EQ(share.mount_path(), base::FilePath());
}

TEST_F(SmbFsShareTest, UnmountOnDisconnect) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });

  EXPECT_CALL(
      observer_,
      OnVolumeMounted(
          MountError::kSuccess,
          AllOf(Property(&file_manager::Volume::type,
                         file_manager::VOLUME_TYPE_SMB),
                Property(&file_manager::Volume::mount_path,
                         base::FilePath(kMountPath)),
                Property(&file_manager::Volume::volume_label, kDisplayName))))
      .Times(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnVolumeUnmounted(
                             MountError::kSuccess,
                             AllOf(Property(&file_manager::Volume::type,
                                            file_manager::VOLUME_TYPE_SMB),
                                   Property(&file_manager::Volume::mount_path,
                                            base::FilePath(kMountPath)))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  share.Mount(
      base::BindLambdaForTesting([&smbfs_receiver](SmbMountResult result) {
        EXPECT_EQ(result, SmbMountResult::kSuccess);

        // Disconnect the Mojo service which should trigger the unmount.
        smbfs_receiver.reset();
      }));
  run_loop.Run();
}

TEST_F(SmbFsShareTest, DisallowCredentialsDialogByDefault) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(MountError::kSuccess, _)).Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(MountError::kSuccess, _)).Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  base::RunLoop run_loop;
  delegate->RequestCredentials(base::BindLambdaForTesting(
      [&run_loop](smbfs::mojom::CredentialsPtr creds) {
        EXPECT_FALSE(creds);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SmbFsShareTest, DisallowCredentialsDialogAfterTimeout) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(MountError::kSuccess, _)).Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(MountError::kSuccess, _)).Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  share.AllowCredentialsRequest();
  // Fast-forward time for the allow state to timeout. The timeout is 5 seconds,
  // so moving forward by 6 will ensure the timeout runs.
  task_environment_.FastForwardBy(base::Seconds(6));

  base::RunLoop run_loop;
  delegate->RequestCredentials(base::BindLambdaForTesting(
      [&run_loop](smbfs::mojom::CredentialsPtr creds) {
        EXPECT_FALSE(creds);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SmbFsShareTest, RemoveSavedCredentials) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(MountError::kSuccess, _)).Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(MountError::kSuccess, _)).Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  EXPECT_CALL(smbfs, RemoveSavedCredentials(_))
      .WillOnce(base::test::RunOnceCallback<0>(true /* success */));
  {
    base::RunLoop run_loop;
    share.RemoveSavedCredentials(
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(SmbFsShareTest, RemoveSavedCredentials_Disconnect) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(MountError::kSuccess, _)).Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(MountError::kSuccess, _)).Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  EXPECT_CALL(smbfs, RemoveSavedCredentials(_))
      .WillOnce([&smbfs_receiver](Unused) { smbfs_receiver.reset(); });
  {
    base::RunLoop run_loop;
    share.RemoveSavedCredentials(
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(SmbFsShareTest, GenerateStableMountIdInput) {
  TestSmbFsImpl smbfs;

  std::string profile_user_hash =
      ProfileHelper::Get()->GetUserIdHashFromProfile(&profile_);

  smbfs::SmbFsMounter::MountOptions options1;
  options1.username = kShareUsername;
  options1.workgroup = kShareWorkgroup;
  SmbFsShare share1(&profile_, SmbUrl(kSharePath), kDisplayName, options1);

  std::string hash_input1 = share1.GenerateStableMountIdInput();
  std::vector<std::string> tokens1 =
      base::SplitString(hash_input1, kMountIdHashSeparator,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  EXPECT_EQ(tokens1.size(), 5u);
  EXPECT_EQ(tokens1[0], profile_user_hash);
  EXPECT_EQ(tokens1[1], SmbUrl(kSharePath).ToString());
  EXPECT_EQ(tokens1[2], "0" /* kerberos */);
  EXPECT_EQ(tokens1[3], kShareWorkgroup);
  EXPECT_EQ(tokens1[4], kShareUsername);

  smbfs::SmbFsMounter::MountOptions options2;
  options2.kerberos_options =
      std::make_optional<smbfs::SmbFsMounter::KerberosOptions>(
          smbfs::SmbFsMounter::KerberosOptions::Source::kKerberos,
          kKerberosIdentity);
  SmbFsShare share2(&profile_, SmbUrl(kSharePath2), kDisplayName, options2);

  std::string hash_input2 = share2.GenerateStableMountIdInput();
  std::vector<std::string> tokens2 =
      base::SplitString(hash_input2, kMountIdHashSeparator,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  EXPECT_EQ(tokens2.size(), 5u);
  EXPECT_EQ(tokens2[0], profile_user_hash);
  EXPECT_EQ(tokens2[1], SmbUrl(kSharePath2).ToString());
  EXPECT_EQ(tokens2[2], "1" /* kerberos */);
  EXPECT_EQ(tokens2[3], "");
  EXPECT_EQ(tokens2[4], "");
}

TEST_F(SmbFsShareTest, GenerateStableMountId) {
  TestSmbFsImpl smbfs;

  smbfs::SmbFsMounter::MountOptions options1;
  options1.username = kShareUsername;
  SmbFsShare share1(&profile_, SmbUrl(kSharePath), kDisplayName, options1);
  std::string mount_id1 = share1.GenerateStableMountId();

  // Check: We get a different hash when options are varied.
  smbfs::SmbFsMounter::MountOptions options2;
  options2.username = kShareUsername2;
  SmbFsShare share2(&profile_, SmbUrl(kSharePath), kDisplayName, options2);
  std::string mount_id2 = share2.GenerateStableMountId();
  EXPECT_TRUE(mount_id1.compare(mount_id2));

  // Check: String is 64 characters long (SHA256 encoded as hex).
  EXPECT_EQ(mount_id1.size(), 64u);
  EXPECT_EQ(mount_id2.size(), 64u);
}

}  // namespace ash::smb_client
