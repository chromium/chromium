// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_service_test_base.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"

namespace ash::smb_client {

namespace {

void SaveMountResult(SmbMountResult* out, SmbMountResult result) {
  *out = result;
}

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

}  // namespace

// MockSmbFsMounter
MockSmbFsMounter::MockSmbFsMounter() = default;
MockSmbFsMounter::~MockSmbFsMounter() = default;

// MockSmbFsImpl
MockSmbFsImpl::MockSmbFsImpl(mojo::PendingReceiver<smbfs::mojom::SmbFs> pending)
    : receiver_(this, std::move(pending)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&MockSmbFsImpl::OnDisconnect, base::Unretained(this)));
}

MockSmbFsImpl::~MockSmbFsImpl() = default;

// SmbServiceBaseTest
SmbServiceBaseTest::TestSmbFsInstance::TestSmbFsInstance(
    mojo::PendingReceiver<smbfs::mojom::SmbFs> pending)
    : mock_smbfs(std::move(pending)) {}

SmbServiceBaseTest::TestSmbFsInstance::~TestSmbFsInstance() = default;

SmbServiceBaseTest::SmbServiceBaseTest() {
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  CHECK(profile_manager_->SetUp());

  {
    auto user_manager_temp = std::make_unique<FakeChromeUserManager>();

    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    user_manager_temp->AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager_temp));
  }

  SmbProviderClient::InitializeFake();
  ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

  disk_mount_manager_ = new disks::FakeDiskMountManager();
  // Takes ownership of |disk_mount_manager_|, but Shutdown() must be called.
  disks::DiskMountManager::InitializeForTesting(disk_mount_manager_);
}

SmbServiceBaseTest::~SmbServiceBaseTest() {
  // The following reset() and Shutdown() are required to override and delete
  // the global dependencies.
  smb_service.reset();
  user_manager_enabler_.reset();
  profile_manager_.reset();
  disks::DiskMountManager::Shutdown();
  SmbProviderClient::Shutdown();
}

void SmbServiceBaseTest::CreateService(TestingProfile* profile) {
  SmbService::DisableShareDiscoveryForTesting();
  file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&BuildVolumeManager));

  smb_service = std::make_unique<SmbService>(
      profile, std::make_unique<base::SimpleTestTickClock>());
}

void SmbServiceBaseTest::ExpectInvalidUrl(const std::string& url) {
  SmbMountResult result = SmbMountResult::kSuccess;
  smb_service->Mount(
      "" /* display_name */, base::FilePath(url), "" /* username */,
      "" /* password */, false /* use_kerberos */,
      false /* should_open_file_manager_after_mount */,
      false /* save_credentials */, base::BindOnce(&SaveMountResult, &result));
  EXPECT_EQ(result, SmbMountResult::kInvalidUrl);
}

void SmbServiceBaseTest::ExpectInvalidSsoUrl(const std::string& url) {
  SmbMountResult result = SmbMountResult::kSuccess;
  smb_service->Mount(
      "" /* display_name */, base::FilePath(url), "" /* username */,
      "" /* password */, true /* use_kerberos */,
      false /* should_open_file_manager_after_mount */,
      false /* save_credentials */, base::BindOnce(&SaveMountResult, &result));
  EXPECT_EQ(result, SmbMountResult::kInvalidSsoUrl);
}

void SmbServiceBaseTest::WaitForSetupComplete() {
  {
    base::RunLoop run_loop;
    smb_service->OnSetupCompleteForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    // Share gathering needs to complete at least once before a share can be
    // mounted.
    base::RunLoop run_loop;
    smb_service->GatherSharesInNetwork(
        base::DoNothing(),
        base::BindLambdaForTesting(
            [&run_loop](const std::vector<SmbUrl>& shares_gathered, bool done) {
              if (done) {
                run_loop.Quit();
              }
            }));
    run_loop.Run();
  }
}

std::unique_ptr<disks::MountPoint> SmbServiceBaseTest::MakeMountPoint(
    const base::FilePath& path) {
  return std::make_unique<disks::MountPoint>(path, disk_mount_manager_);
}

std::unique_ptr<SmbServiceBaseTest::TestSmbFsInstance>
SmbServiceBaseTest::MountBasicShare(const std::string& share_path,
                                    const std::string& mount_path,
                                    SmbService::MountResponse callback) {
  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  auto instance = std::make_unique<TestSmbFsInstance>(
      smbfs_remote.BindNewPipeAndPassReceiver());

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  // Use a NiceMock<> so that the ON_CALL below doesn't complain.
  auto mock_mounter = std::make_unique<NiceMock<MockSmbFsMounter>>();

  smb_service->SetSmbFsMounterCreationCallbackForTesting(
      base::BindLambdaForTesting([&mock_mounter, &smbfs_host_delegate](
                                     const std::string& share_path,
                                     const std::string& mount_dir_name,
                                     const SmbFsShare::MountOptions& options,
                                     smbfs::SmbFsHost::Delegate* delegate)
                                     -> std::unique_ptr<smbfs::SmbFsMounter> {
        smbfs_host_delegate = delegate;
        return std::move(mock_mounter);
      }));

  // Use ON_CALL instead of EXPECT_CALL because there might be a failure
  // earlier in the mount process and this won't be called.
  ON_CALL(*mock_mounter, Mount(_))
      .WillByDefault(
          [this, &smbfs_host_delegate, &smbfs_remote, &instance,
           &mount_path](smbfs::SmbFsMounter::DoneCallback mount_callback) {
            std::move(mount_callback)
                .Run(smbfs::mojom::MountError::kOk,
                     std::make_unique<smbfs::SmbFsHost>(
                         MakeMountPoint(base::FilePath(mount_path)),
                         smbfs_host_delegate, std::move(smbfs_remote),
                         instance->delegate.BindNewPipeAndPassReceiver()));
          });

  base::RunLoop run_loop;
  smb_service->Mount(
      kDisplayName, base::FilePath(share_path), "" /* username */,
      "" /* password */, false /* use_kerberos */,
      false /* should_open_file_manager_after_mount */,
      false /* save_credentials */,
      base::BindLambdaForTesting([&run_loop, &callback](SmbMountResult result) {
        std::move(callback).Run(result);
        run_loop.Quit();
      }));
  run_loop.Run();

  return instance;
}

}  // namespace ash::smb_client
