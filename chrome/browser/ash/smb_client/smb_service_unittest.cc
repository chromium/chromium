// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_service.h"

#include <stddef.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/fake_registry.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/smb_client/smb_file_system_id.h"
#include "chrome/browser/ash/smb_client/smb_persisted_share_registry.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/smbprovider/fake_smb_provider_client.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/smbfs/smbfs_host.h"
#include "chromeos/ash/components/smbfs/smbfs_mounter.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace ash {
namespace smb_client {

namespace {

constexpr char kTestUser[] = "foobar";
constexpr char kTestPassword[] = "my_secret_password";
constexpr char kTestDomain[] = "EXAMPLE.COM";
constexpr char kSharePath[] = "\\\\server\\foobar";
constexpr char kSharePath2[] = "\\\\server2\\second_share";
constexpr char kShareUrl[] = "smb://server/foobar";
constexpr char kInvalidShareUrl[] = "smb://server";
constexpr char kDisplayName[] = "My Share";
constexpr char kMountPath[] = "/share/mount/path";
constexpr char kMountPath2[] = "/share/mount/second_path";

constexpr char kTestADUser[] = "ad-test-user";
constexpr char kTestADDomain[] = "foobar.corp";
constexpr char kTestADGuid[] = "ad-user-guid";

void SaveMountResult(SmbMountResult* out, SmbMountResult result) {
  *out = result;
}

class MockSmbFsMounter : public smbfs::SmbFsMounter {
 public:
  MOCK_METHOD(void,
              Mount,
              (smbfs::SmbFsMounter::DoneCallback callback),
              (override));
};

class MockSmbFsImpl : public smbfs::mojom::SmbFs {
 public:
  explicit MockSmbFsImpl(mojo::PendingReceiver<smbfs::mojom::SmbFs> pending)
      : receiver_(this, std::move(pending)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockSmbFsImpl::OnDisconnect, base::Unretained(this)));
  }

  // Mojo disconnection handler.
  MOCK_METHOD(void, OnDisconnect, (), ());

  // smbfs::mojom::SmbFs overrides.
  MOCK_METHOD(void,
              RemoveSavedCredentials,
              (RemoveSavedCredentialsCallback),
              (override));

  MOCK_METHOD(void,
              DeleteRecursively,
              (const base::FilePath&, DeleteRecursivelyCallback),
              (override));

 private:
  mojo::Receiver<smbfs::mojom::SmbFs> receiver_;
};

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

class SmbServiceWithSmbfsTest : public testing::Test {
 protected:
  // Mojo endpoints owned by the smbfs instance.
  struct TestSmbFsInstance {
    explicit TestSmbFsInstance(
        mojo::PendingReceiver<smbfs::mojom::SmbFs> pending)
        : mock_smbfs(std::move(pending)) {}

    MockSmbFsImpl mock_smbfs;
    mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;
  };

  SmbServiceWithSmbfsTest() {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());

    std::unique_ptr<FakeChromeUserManager> user_manager_temp =
        std::make_unique<FakeChromeUserManager>();

    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    user_manager_temp->AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));

    ad_profile_ = profile_manager_->CreateTestingProfile(
        base::StrCat({kTestADUser, "@", kTestADDomain}));
    user_manager_temp->AddUserWithAffiliationAndTypeAndProfile(
        AccountId::AdFromUserEmailObjGuid(ad_profile_->GetProfileUserName(),
                                          kTestADGuid),
        false, user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY, ad_profile_);

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager_temp));

    SmbProviderClient::InitializeFake();
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    // Takes ownership of |disk_mount_manager_|, but Shutdown() must be called.
    disks::DiskMountManager::InitializeForTesting(disk_mount_manager_);
  }

  ~SmbServiceWithSmbfsTest() override {
    smb_service_.reset();
    user_manager_enabler_.reset();
    profile_manager_.reset();
    disks::DiskMountManager::Shutdown();
    ConciergeClient::Shutdown();
    SmbProviderClient::Shutdown();
  }

  void CreateService(TestingProfile* profile) {
    SmbService::DisableShareDiscoveryForTesting();
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating(&BuildVolumeManager));

    // Create smb service.
    smb_service_ = std::make_unique<SmbService>(
        profile, std::make_unique<base::SimpleTestTickClock>());
  }

  void ExpectInvalidUrl(const std::string& url) {
    SmbMountResult result = SmbMountResult::kSuccess;
    smb_service_->Mount("" /* display_name */, base::FilePath(url),
                        "" /* username */, "" /* password */,
                        false /* use_kerberos */,
                        false /* should_open_file_manager_after_mount */,
                        false /* save_credentials */,
                        base::BindOnce(&SaveMountResult, &result));
    EXPECT_EQ(result, SmbMountResult::kInvalidUrl);
  }

  void ExpectInvalidSsoUrl(const std::string& url) {
    SmbMountResult result = SmbMountResult::kSuccess;
    smb_service_->Mount("" /* display_name */, base::FilePath(url),
                        "" /* username */, "" /* password */,
                        true /* use_kerberos */,
                        false /* should_open_file_manager_after_mount */,
                        false /* save_credentials */,
                        base::BindOnce(&SaveMountResult, &result));
    EXPECT_EQ(result, SmbMountResult::kInvalidSsoUrl);
  }

  void WaitForSetupComplete() {
    {
      base::RunLoop run_loop;
      smb_service_->OnSetupCompleteForTesting(run_loop.QuitClosure());
      run_loop.Run();
    }
    {
      // Share gathering needs to complete at least once before a share can be
      // mounted.
      base::RunLoop run_loop;
      smb_service_->GatherSharesInNetwork(
          base::DoNothing(),
          base::BindLambdaForTesting(
              [&run_loop](const std::vector<SmbUrl>& shares_gathered,
                          bool done) {
                if (done) {
                  run_loop.Quit();
                }
              }));
      run_loop.Run();
    }
  }

  std::unique_ptr<disks::MountPoint> MakeMountPoint(
      const base::FilePath& path) {
    return std::make_unique<disks::MountPoint>(path, disk_mount_manager_);
  }

  // Helper function for creating a basic smbfs mount with an empty
  // username/password.
  std::unique_ptr<TestSmbFsInstance> MountBasicShare(
      const std::string& share_path,
      const std::string& mount_path,
      SmbService::MountResponse callback) {
    mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
    std::unique_ptr<TestSmbFsInstance> instance =
        std::make_unique<TestSmbFsInstance>(
            smbfs_remote.BindNewPipeAndPassReceiver());

    smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
    // Use a NiceMock<> so that the ON_CALL below doesn't complain.
    std::unique_ptr<MockSmbFsMounter> mock_mounter =
        std::make_unique<NiceMock<MockSmbFsMounter>>();

    smb_service_->SetSmbFsMounterCreationCallbackForTesting(
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
    smb_service_->Mount(kDisplayName, base::FilePath(share_path),
                        "" /* username */, "" /* password */,
                        false /* use_kerberos */,
                        false /* should_open_file_manager_after_mount */,
                        false /* save_credentials */,
                        base::BindLambdaForTesting(
                            [&run_loop, &callback](SmbMountResult result) {
                              std::move(callback).Run(result);
                              run_loop.Quit();
                            }));
    run_loop.Run();

    return instance;
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<file_manager::FakeDiskMountManager, ExperimentalAsh>
      disk_mount_manager_ = new file_manager::FakeDiskMountManager;

  // Not owned.
  raw_ptr<TestingProfile, ExperimentalAsh> profile_ = nullptr;

  // Not owned.
  raw_ptr<TestingProfile, ExperimentalAsh> ad_profile_ = nullptr;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<SmbService> smb_service_;
};

TEST_F(SmbServiceWithSmbfsTest, InvalidUrls) {
  CreateService(profile_);

  ExpectInvalidUrl("");
  ExpectInvalidUrl("foo");
  ExpectInvalidUrl("\\foo");
  ExpectInvalidUrl("\\\\foo");
  ExpectInvalidUrl("\\\\foo\\");
  ExpectInvalidUrl("file://foo/bar");
  ExpectInvalidUrl("smb://foo");
  ExpectInvalidUrl("smb://user@password:foo");
  ExpectInvalidUrl("smb:\\\\foo\\bar");
  ExpectInvalidUrl("//foo/bar");
}

TEST_F(SmbServiceWithSmbfsTest, InvalidSsoUrls) {
  CreateService(profile_);

  ExpectInvalidSsoUrl("\\\\192.168.1.1\\foo");
  ExpectInvalidSsoUrl("\\\\[0:0:0:0:0:0:0:1]\\foo");
  ExpectInvalidSsoUrl("\\\\[::1]\\foo");
  ExpectInvalidSsoUrl("smb://192.168.1.1/foo");
  ExpectInvalidSsoUrl("smb://[0:0:0:0:0:0:0:1]/foo");
  ExpectInvalidSsoUrl("smb://[::1]/foo");
}

TEST_F(SmbServiceWithSmbfsTest, Mount) {
  CreateService(profile_);
  WaitForSetupComplete();

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  std::unique_ptr<MockSmbFsMounter> mock_mounter =
      std::make_unique<MockSmbFsMounter>();
  smb_service_->SetSmbFsMounterCreationCallbackForTesting(
      base::BindLambdaForTesting([&mock_mounter, &smbfs_host_delegate](
                                     const std::string& share_path,
                                     const std::string& mount_dir_name,
                                     const SmbFsShare::MountOptions& options,
                                     smbfs::SmbFsHost::Delegate* delegate)
                                     -> std::unique_ptr<smbfs::SmbFsMounter> {
        EXPECT_EQ(share_path, kShareUrl);
        EXPECT_EQ(options.username, kTestUser);
        EXPECT_TRUE(options.workgroup.empty());
        EXPECT_EQ(options.password, kTestPassword);
        EXPECT_TRUE(options.allow_ntlm);
        EXPECT_FALSE(options.kerberos_options);
        smbfs_host_delegate = delegate;
        return std::move(mock_mounter);
      }));
  EXPECT_CALL(*mock_mounter, Mount(_))
      .WillOnce(
          [this, &smbfs_host_delegate, &smbfs_remote,
           &smbfs_delegate_remote](smbfs::SmbFsMounter::DoneCallback callback) {
            std::move(callback).Run(
                smbfs::mojom::MountError::kOk,
                std::make_unique<smbfs::SmbFsHost>(
                    MakeMountPoint(base::FilePath(kMountPath)),
                    smbfs_host_delegate, std::move(smbfs_remote),
                    smbfs_delegate_remote.BindNewPipeAndPassReceiver()));
          });

  base::RunLoop run_loop;
  smb_service_->Mount(
      kDisplayName, base::FilePath(kSharePath), kTestUser, kTestPassword,
      false /* use_kerberos */,
      false /* should_open_file_manager_after_mount */,
      false /* save_credentials */,
      base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
        EXPECT_EQ(SmbMountResult::kSuccess, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Expect that the filesystem mount path is registered.
  std::vector<storage::MountPoints::MountPointInfo> mount_points;
  storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
      &mount_points);
  bool found = false;
  for (const auto& info : mount_points) {
    if (info.path == base::FilePath(kMountPath)) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);

  // Check that the SmbFsShare can be accessed.
  const base::FilePath mount_path(kMountPath);
  SmbFsShare* share = smb_service_->GetSmbFsShareForPath(mount_path);
  ASSERT_TRUE(share);
  EXPECT_EQ(share->mount_path(), mount_path);
  EXPECT_EQ(share->share_url().ToString(), kShareUrl);

  // Check that the share was saved.
  SmbPersistedShareRegistry registry(profile_);
  absl::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->share_url().ToString(), kShareUrl);
  EXPECT_EQ(info->display_name(), kDisplayName);
  EXPECT_EQ(info->username(), kTestUser);
  EXPECT_TRUE(info->workgroup().empty());
  EXPECT_FALSE(info->use_kerberos());

  // Unmounting should remove the saved share. Since |save_credentials| was
  // false, there should be no request to smbfs.
  EXPECT_CALL(smbfs_impl, RemoveSavedCredentials(_)).Times(0);
  smb_service_->UnmountSmbFs(base::FilePath(kMountPath));
  info = registry.Get(SmbUrl(kShareUrl));
  EXPECT_FALSE(info);
  EXPECT_TRUE(registry.GetAll().empty());
}

TEST_F(SmbServiceWithSmbfsTest, Mount_SaveCredentials) {
  CreateService(profile_);
  WaitForSetupComplete();

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  std::unique_ptr<MockSmbFsMounter> mock_mounter =
      std::make_unique<MockSmbFsMounter>();
  smb_service_->SetSmbFsMounterCreationCallbackForTesting(
      base::BindLambdaForTesting([&mock_mounter, &smbfs_host_delegate](
                                     const std::string& share_path,
                                     const std::string& mount_dir_name,
                                     const SmbFsShare::MountOptions& options,
                                     smbfs::SmbFsHost::Delegate* delegate)
                                     -> std::unique_ptr<smbfs::SmbFsMounter> {
        EXPECT_EQ(share_path, kShareUrl);
        EXPECT_EQ(options.username, kTestUser);
        EXPECT_TRUE(options.workgroup.empty());
        EXPECT_EQ(options.password, kTestPassword);
        EXPECT_FALSE(options.kerberos_options);
        EXPECT_TRUE(options.save_restore_password);
        EXPECT_FALSE(options.account_hash.empty());
        EXPECT_FALSE(options.password_salt.empty());
        smbfs_host_delegate = delegate;
        return std::move(mock_mounter);
      }));
  EXPECT_CALL(*mock_mounter, Mount(_))
      .WillOnce(
          [this, &smbfs_host_delegate, &smbfs_remote,
           &smbfs_delegate_remote](smbfs::SmbFsMounter::DoneCallback callback) {
            std::move(callback).Run(
                smbfs::mojom::MountError::kOk,
                std::make_unique<smbfs::SmbFsHost>(
                    MakeMountPoint(base::FilePath(kMountPath)),
                    smbfs_host_delegate, std::move(smbfs_remote),
                    smbfs_delegate_remote.BindNewPipeAndPassReceiver()));
          });

  base::RunLoop run_loop;
  smb_service_->Mount(
      kDisplayName, base::FilePath(kSharePath), kTestUser, kTestPassword,
      false /* use_kerberos */,
      false /* should_open_file_manager_after_mount */,
      true /* save_credentials */,
      base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
        EXPECT_EQ(SmbMountResult::kSuccess, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Check that the share was saved.
  SmbPersistedShareRegistry registry(profile_);
  absl::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->share_url().ToString(), kShareUrl);
  EXPECT_EQ(info->display_name(), kDisplayName);
  EXPECT_EQ(info->username(), kTestUser);
  EXPECT_TRUE(info->workgroup().empty());
  EXPECT_FALSE(info->use_kerberos());
  EXPECT_FALSE(info->password_salt().empty());
}

TEST_F(SmbServiceWithSmbfsTest, Mount_ActiveDirectory) {
  CreateService(ad_profile_);
  WaitForSetupComplete();

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  std::unique_ptr<MockSmbFsMounter> mock_mounter =
      std::make_unique<MockSmbFsMounter>();
  smb_service_->SetSmbFsMounterCreationCallbackForTesting(
      base::BindLambdaForTesting([&mock_mounter, &smbfs_host_delegate](
                                     const std::string& share_path,
                                     const std::string& mount_dir_name,
                                     const SmbFsShare::MountOptions& options,
                                     smbfs::SmbFsHost::Delegate* delegate)
                                     -> std::unique_ptr<smbfs::SmbFsMounter> {
        EXPECT_EQ(share_path, kShareUrl);
        // Username/workgroup/password are ignored when using Active
        // Directory. Username/workgroup are derived from the account email
        // address. Password is unused and dropped.
        EXPECT_EQ(options.username, kTestADUser);
        // Workgroup/domain is converted to upper-case.
        EXPECT_EQ(options.workgroup, base::ToUpperASCII(kTestADDomain));
        EXPECT_TRUE(options.password.empty());
        EXPECT_TRUE(options.allow_ntlm);
        EXPECT_EQ(
            options.kerberos_options->source,
            smbfs::SmbFsMounter::KerberosOptions::Source::kActiveDirectory);
        EXPECT_EQ(options.kerberos_options->identity, kTestADGuid);
        smbfs_host_delegate = delegate;
        return std::move(mock_mounter);
      }));
  EXPECT_CALL(*mock_mounter, Mount(_))
      .WillOnce(
          [this, &smbfs_host_delegate, &smbfs_remote,
           &smbfs_delegate_remote](smbfs::SmbFsMounter::DoneCallback callback) {
            std::move(callback).Run(
                smbfs::mojom::MountError::kOk,
                std::make_unique<smbfs::SmbFsHost>(
                    MakeMountPoint(base::FilePath(kMountPath)),
                    smbfs_host_delegate, std::move(smbfs_remote),
                    smbfs_delegate_remote.BindNewPipeAndPassReceiver()));
          });

  base::RunLoop run_loop;
  smb_service_->Mount(
      kDisplayName, base::FilePath(kSharePath),
      base::StrCat({kTestUser, "@", kTestDomain}), kTestPassword,
      true /* use_kerberos */, false /* should_open_file_manager_after_mount */,
      false /* save_credentials */,
      base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
        EXPECT_EQ(SmbMountResult::kSuccess, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Check that the share was saved.
  SmbPersistedShareRegistry registry(ad_profile_);
  absl::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->share_url().ToString(), kShareUrl);
  EXPECT_EQ(info->display_name(), kDisplayName);
  EXPECT_EQ(info->username(), kTestADUser);
  // Workgroup/domain is converted to upper-case.
  EXPECT_EQ(info->workgroup(), base::ToUpperASCII(kTestADDomain));
  EXPECT_TRUE(info->use_kerberos());
}

TEST_F(SmbServiceWithSmbfsTest, MountPreconfigured) {
  const char kPremountPath[] = "smb://preconfigured/share";
  const char kPreconfiguredShares[] =
      R"([{"mode":"pre_mount","share_url":"\\\\preconfigured\\share"}])";
  auto parsed_shares = base::JSONReader::Read(kPreconfiguredShares);
  ASSERT_TRUE(parsed_shares);
  profile_->GetPrefs()->Set(prefs::kNetworkFileSharesPreconfiguredShares,
                            *parsed_shares);

  CreateService(profile_);

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  std::unique_ptr<MockSmbFsMounter> mock_mounter =
      std::make_unique<MockSmbFsMounter>();
  smb_service_->SetSmbFsMounterCreationCallbackForTesting(
      base::BindLambdaForTesting(
          [&mock_mounter, &smbfs_host_delegate, kPremountPath](
              const std::string& share_path, const std::string& mount_dir_name,
              const SmbFsShare::MountOptions& options,
              smbfs::SmbFsHost::Delegate* delegate)
              -> std::unique_ptr<smbfs::SmbFsMounter> {
            EXPECT_EQ(share_path, kPremountPath);
            EXPECT_TRUE(options.username.empty());
            EXPECT_TRUE(options.workgroup.empty());
            EXPECT_TRUE(options.password.empty());
            EXPECT_FALSE(options.kerberos_options);
            smbfs_host_delegate = delegate;
            return std::move(mock_mounter);
          }));

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_mounter, Mount(_))
      .WillOnce([this, &smbfs_host_delegate, &smbfs_remote,
                 &smbfs_delegate_remote,
                 &run_loop](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            std::make_unique<smbfs::SmbFsHost>(
                MakeMountPoint(base::FilePath(kMountPath)), smbfs_host_delegate,
                std::move(smbfs_remote),
                smbfs_delegate_remote.BindNewPipeAndPassReceiver()));
        run_loop.Quit();
      });

  run_loop.Run();
}

TEST_F(SmbServiceWithSmbfsTest, MountInvalidPreconfigured) {
  const char kPreconfiguredShares[] =
      R"([{"mode":"pre_mount","share_url":"\\\\preconfigured"}])";
  auto parsed_shares = base::JSONReader::Read(kPreconfiguredShares);
  ASSERT_TRUE(parsed_shares);
  profile_->GetPrefs()->Set(prefs::kNetworkFileSharesPreconfiguredShares,
                            *parsed_shares);

  CreateService(profile_);

  base::RunLoop run_loop;
  smb_service_->SetRestoredShareMountDoneCallbackForTesting(
      base::BindLambdaForTesting([&run_loop](SmbMountResult mount_result,
                                             const base::FilePath& mount_path) {
        EXPECT_EQ(mount_result, SmbMountResult::kInvalidUrl);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(SmbServiceWithSmbfsTest, MountSaved) {
  const std::vector<uint8_t> kSalt = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  // Save share in profile.
  {
    SmbPersistedShareRegistry registry(profile_);
    SmbShareInfo info(SmbUrl(kShareUrl), kDisplayName, kTestUser, kTestDomain,
                      false /* use_kerberos */, kSalt);
    registry.Save(info);
  }

  CreateService(profile_);

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  std::unique_ptr<MockSmbFsMounter> mock_mounter =
      std::make_unique<MockSmbFsMounter>();
  smb_service_->SetSmbFsMounterCreationCallbackForTesting(
      base::BindLambdaForTesting([&mock_mounter, &smbfs_host_delegate, kSalt](
                                     const std::string& share_path,
                                     const std::string& mount_dir_name,
                                     const SmbFsShare::MountOptions& options,
                                     smbfs::SmbFsHost::Delegate* delegate)
                                     -> std::unique_ptr<smbfs::SmbFsMounter> {
        EXPECT_EQ(share_path, kShareUrl);
        EXPECT_EQ(options.username, kTestUser);
        EXPECT_EQ(options.workgroup, kTestDomain);
        EXPECT_TRUE(options.password.empty());
        EXPECT_TRUE(options.allow_ntlm);
        EXPECT_FALSE(options.kerberos_options);
        EXPECT_TRUE(options.save_restore_password);
        EXPECT_FALSE(options.account_hash.empty());
        EXPECT_EQ(options.password_salt, kSalt);
        smbfs_host_delegate = delegate;
        return std::move(mock_mounter);
      }));

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_mounter, Mount(_))
      .WillOnce([this, &smbfs_host_delegate, &smbfs_remote,
                 &smbfs_delegate_remote,
                 &run_loop](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            std::make_unique<smbfs::SmbFsHost>(
                MakeMountPoint(base::FilePath(kMountPath)), smbfs_host_delegate,
                std::move(smbfs_remote),
                smbfs_delegate_remote.BindNewPipeAndPassReceiver()));
        run_loop.Quit();
      });

  run_loop.Run();

  // Unmounting should remove the saved share, and ask smbfs to remove any saved
  // credentials.
  base::RunLoop run_loop2;
  EXPECT_CALL(smbfs_impl, RemoveSavedCredentials(_))
      .WillOnce(
          [](smbfs::mojom::SmbFs::RemoveSavedCredentialsCallback callback) {
            std::move(callback).Run(true /* success */);
          });
  EXPECT_CALL(smbfs_impl, OnDisconnect())
      .WillOnce(base::test::RunClosure(run_loop2.QuitClosure()));
  smb_service_->UnmountSmbFs(base::FilePath(kMountPath));
  run_loop2.Run();

  SmbPersistedShareRegistry registry(profile_);
  absl::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  EXPECT_FALSE(info);
  EXPECT_TRUE(registry.GetAll().empty());
}

TEST_F(SmbServiceWithSmbfsTest, MountInvalidSaved) {
  const std::vector<uint8_t> kSalt = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  // Save an (invalid) share in profile. This can't occur in practice.
  {
    SmbPersistedShareRegistry registry(profile_);
    SmbShareInfo info(SmbUrl(kInvalidShareUrl), kDisplayName, kTestUser,
                      kTestDomain, /*use_kerberos=*/false, kSalt);
    registry.Save(info);
  }

  CreateService(profile_);

  base::RunLoop run_loop;
  smb_service_->SetRestoredShareMountDoneCallbackForTesting(
      base::BindLambdaForTesting([&run_loop](SmbMountResult mount_result,
                                             const base::FilePath& mount_path) {
        EXPECT_EQ(mount_result, SmbMountResult::kInvalidUrl);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(SmbServiceWithSmbfsTest, MountExcessiveShares) {
  // The maximum number of smbfs shares that can be mounted simultaneously.
  // Should match the definition in smb_service.cc.
  const size_t kMaxSmbFsShares = 16;
  CreateService(profile_);
  WaitForSetupComplete();

  // Check: It is possible to mount the maximum number of shares.
  for (size_t i = 0; i < kMaxSmbFsShares; ++i) {
    const std::string share_path =
        std::string(kSharePath) + base::NumberToString(i);
    const std::string mount_path =
        std::string(kMountPath) + base::NumberToString(i);
    std::ignore = MountBasicShare(share_path, mount_path,
                                  base::BindOnce([](SmbMountResult result) {
                                    EXPECT_EQ(SmbMountResult::kSuccess, result);
                                  }));
  }

  // Check: After mounting the maximum number of shares, requesting to mount an
  // additional share should fail.
  const std::string share_path =
      std::string(kSharePath) + base::NumberToString(kMaxSmbFsShares);
  const std::string mount_path =
      std::string(kMountPath) + base::NumberToString(kMaxSmbFsShares);
  std::ignore = MountBasicShare(
      share_path, mount_path, base::BindOnce([](SmbMountResult result) {
        EXPECT_EQ(SmbMountResult::kTooManyOpened, result);
      }));
}

TEST_F(SmbServiceWithSmbfsTest, GetSmbFsShareForPath) {
  CreateService(profile_);
  WaitForSetupComplete();

  std::ignore = MountBasicShare(kSharePath, kMountPath,
                                base::BindOnce([](SmbMountResult result) {
                                  EXPECT_EQ(SmbMountResult::kSuccess, result);
                                }));
  std::ignore = MountBasicShare(kSharePath2, kMountPath2,
                                base::BindOnce([](SmbMountResult result) {
                                  EXPECT_EQ(SmbMountResult::kSuccess, result);
                                }));

  SmbFsShare* share =
      smb_service_->GetSmbFsShareForPath(base::FilePath(kMountPath));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath));
  share = smb_service_->GetSmbFsShareForPath(
      base::FilePath(kMountPath).Append("foo"));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath));

  share = smb_service_->GetSmbFsShareForPath(base::FilePath(kMountPath2));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath2));
  share = smb_service_->GetSmbFsShareForPath(
      base::FilePath(kMountPath2).Append("bar/baz"));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath2));

  EXPECT_FALSE(
      smb_service_->GetSmbFsShareForPath(base::FilePath("/share/mount")));
  EXPECT_FALSE(smb_service_->GetSmbFsShareForPath(
      base::FilePath("/share/mount/third_path")));
}

TEST_F(SmbServiceWithSmbfsTest, MountDuplicate) {
  CreateService(profile_);
  WaitForSetupComplete();

  std::ignore = MountBasicShare(kSharePath, kMountPath,
                                base::BindOnce([](SmbMountResult result) {
                                  EXPECT_EQ(SmbMountResult::kSuccess, result);
                                }));

  // A second mount with the same share path should fail.
  std::ignore = MountBasicShare(
      kSharePath, kMountPath2, base::BindOnce([](SmbMountResult result) {
        EXPECT_EQ(SmbMountResult::kMountExists, result);
      }));

  // Unmounting and mounting again should succeed.
  smb_service_->UnmountSmbFs(base::FilePath(kMountPath));
  std::ignore = MountBasicShare(kSharePath, kMountPath2,
                                base::BindOnce([](SmbMountResult result) {
                                  EXPECT_EQ(SmbMountResult::kSuccess, result);
                                }));
}

}  // namespace smb_client
}  // namespace ash
