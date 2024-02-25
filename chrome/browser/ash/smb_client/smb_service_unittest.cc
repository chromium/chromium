// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_service.h"

#include "base/json/json_reader.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ash/smb_client/smb_service_test_base.h"
#include "chrome/common/pref_names.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {

namespace {

inline constexpr char kTestUser[] = "foobar";
inline constexpr char kTestPassword[] = "my_secret_password";
inline constexpr char kTestDomain[] = "EXAMPLE.COM";
inline constexpr char kSharePath2[] = "\\\\server2\\second_share";
inline constexpr char kShareUrl[] = "smb://server/foobar";
inline constexpr char kInvalidShareUrl[] = "smb://server";
inline constexpr char kMountPath2[] = "/share/mount/second_path";

}  // namespace

class SmbServiceWithSmbfsTest : public SmbServiceBaseTest {};

TEST_F(SmbServiceWithSmbfsTest, InvalidUrls) {
  CreateService(profile());

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
  CreateService(profile());

  ExpectInvalidSsoUrl("\\\\192.168.1.1\\foo");
  ExpectInvalidSsoUrl("\\\\[0:0:0:0:0:0:0:1]\\foo");
  ExpectInvalidSsoUrl("\\\\[::1]\\foo");
  ExpectInvalidSsoUrl("smb://192.168.1.1/foo");
  ExpectInvalidSsoUrl("smb://[0:0:0:0:0:0:0:1]/foo");
  ExpectInvalidSsoUrl("smb://[::1]/foo");
}

TEST_F(SmbServiceWithSmbfsTest, Mount) {
  CreateService(profile());
  WaitForSetupComplete();

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  auto mock_mounter = std::make_unique<MockSmbFsMounter>();
  smb_service->SetSmbFsMounterCreationCallbackForTesting(
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
  smb_service->Mount(
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
  SmbFsShare* share = smb_service->GetSmbFsShareForPath(mount_path);
  ASSERT_TRUE(share);
  EXPECT_EQ(share->mount_path(), mount_path);
  EXPECT_EQ(share->share_url().ToString(), kShareUrl);

  // Check that the share was saved.
  SmbPersistedShareRegistry registry(profile());
  std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->share_url().ToString(), kShareUrl);
  EXPECT_EQ(info->display_name(), kDisplayName);
  EXPECT_EQ(info->username(), kTestUser);
  EXPECT_TRUE(info->workgroup().empty());
  EXPECT_FALSE(info->use_kerberos());

  // Unmounting should remove the saved share. Since |save_credentials| was
  // false, there should be no request to smbfs.
  EXPECT_CALL(smbfs_impl, RemoveSavedCredentials(_)).Times(0);
  smb_service->UnmountSmbFs(base::FilePath(kMountPath));
  info = registry.Get(SmbUrl(kShareUrl));
  EXPECT_FALSE(info);
  EXPECT_TRUE(registry.GetAll().empty());
}

TEST_F(SmbServiceWithSmbfsTest, Mount_SaveCredentials) {
  CreateService(profile());
  WaitForSetupComplete();

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  auto mock_mounter = std::make_unique<MockSmbFsMounter>();
  smb_service->SetSmbFsMounterCreationCallbackForTesting(
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
  smb_service->Mount(
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
  SmbPersistedShareRegistry registry(profile());
  std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  ASSERT_TRUE(info);
  EXPECT_EQ(info->share_url().ToString(), kShareUrl);
  EXPECT_EQ(info->display_name(), kDisplayName);
  EXPECT_EQ(info->username(), kTestUser);
  EXPECT_TRUE(info->workgroup().empty());
  EXPECT_FALSE(info->use_kerberos());
  EXPECT_FALSE(info->password_salt().empty());
}

TEST_F(SmbServiceWithSmbfsTest, MountPreconfigured) {
  const char kPremountPath[] = "smb://preconfigured/share";
  const char kPreconfiguredShares[] =
      R"([{"mode":"pre_mount","share_url":"\\\\preconfigured\\share"}])";
  auto parsed_shares = base::JSONReader::Read(kPreconfiguredShares);
  ASSERT_TRUE(parsed_shares);
  profile()->GetPrefs()->Set(prefs::kNetworkFileSharesPreconfiguredShares,
                             *parsed_shares);

  CreateService(profile());

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  auto mock_mounter = std::make_unique<MockSmbFsMounter>();
  smb_service->SetSmbFsMounterCreationCallbackForTesting(
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
  profile()->GetPrefs()->Set(prefs::kNetworkFileSharesPreconfiguredShares,
                             *parsed_shares);

  CreateService(profile());

  base::RunLoop run_loop;
  smb_service->SetRestoredShareMountDoneCallbackForTesting(
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
    SmbPersistedShareRegistry registry(profile());
    SmbShareInfo info(SmbUrl(kShareUrl), kDisplayName, kTestUser, kTestDomain,
                      false /* use_kerberos */, kSalt);
    registry.Save(info);
  }

  CreateService(profile());

  mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
  MockSmbFsImpl smbfs_impl(smbfs_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<smbfs::mojom::SmbFsDelegate> smbfs_delegate_remote;

  smbfs::SmbFsHost::Delegate* smbfs_host_delegate = nullptr;
  auto mock_mounter = std::make_unique<MockSmbFsMounter>();
  smb_service->SetSmbFsMounterCreationCallbackForTesting(
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
  smb_service->UnmountSmbFs(base::FilePath(kMountPath));
  run_loop2.Run();

  SmbPersistedShareRegistry registry(profile());
  std::optional<SmbShareInfo> info = registry.Get(SmbUrl(kShareUrl));
  EXPECT_FALSE(info);
  EXPECT_TRUE(registry.GetAll().empty());
}

TEST_F(SmbServiceWithSmbfsTest, MountInvalidSaved) {
  const std::vector<uint8_t> kSalt = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  // Save an (invalid) share in profile. This can't occur in practice.
  {
    SmbPersistedShareRegistry registry(profile());
    SmbShareInfo info(SmbUrl(kInvalidShareUrl), kDisplayName, kTestUser,
                      kTestDomain, /*use_kerberos=*/false, kSalt);
    registry.Save(info);
  }

  CreateService(profile());

  base::RunLoop run_loop;
  smb_service->SetRestoredShareMountDoneCallbackForTesting(
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
  CreateService(profile());
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
  CreateService(profile());
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
      smb_service->GetSmbFsShareForPath(base::FilePath(kMountPath));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath));
  share = smb_service->GetSmbFsShareForPath(
      base::FilePath(kMountPath).Append("foo"));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath));

  share = smb_service->GetSmbFsShareForPath(base::FilePath(kMountPath2));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath2));
  share = smb_service->GetSmbFsShareForPath(
      base::FilePath(kMountPath2).Append("bar/baz"));
  EXPECT_EQ(share->mount_path(), base::FilePath(kMountPath2));

  EXPECT_FALSE(
      smb_service->GetSmbFsShareForPath(base::FilePath("/share/mount")));
  EXPECT_FALSE(smb_service->GetSmbFsShareForPath(
      base::FilePath("/share/mount/third_path")));
}

TEST_F(SmbServiceWithSmbfsTest, MountDuplicate) {
  CreateService(profile());
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
  smb_service->UnmountSmbFs(base::FilePath(kMountPath));
  std::ignore = MountBasicShare(kSharePath, kMountPath2,
                                base::BindOnce([](SmbMountResult result) {
                                  EXPECT_EQ(SmbMountResult::kSuccess, result);
                                }));
}

TEST_F(SmbServiceWithSmbfsTest, IsAnySmbShareAdded) {
  CreateService(profile());
  WaitForSetupComplete();
  EXPECT_FALSE(smb_service->IsAnySmbShareConfigured());

  // Add a share
  std::ignore = MountBasicShare(kSharePath, kMountPath,
                                base::BindOnce([](SmbMountResult result) {
                                  EXPECT_EQ(SmbMountResult::kSuccess, result);
                                }));

  EXPECT_TRUE(smb_service->IsAnySmbShareConfigured());
}

TEST_F(SmbServiceWithSmbfsTest, IsAnySmbShareConfigured) {
  // Add a preconfigured share
  const char kPreconfiguredShares[] =
      R"([{"mode":"pre_mount","share_url":"\\\\preconfigured\\share"}])";
  auto parsed_shares = base::JSONReader::Read(kPreconfiguredShares);
  ASSERT_TRUE(parsed_shares);
  profile()->GetPrefs()->Set(prefs::kNetworkFileSharesPreconfiguredShares,
                             *parsed_shares);

  CreateService(profile());
  EXPECT_TRUE(smb_service->IsAnySmbShareConfigured());
}

}  // namespace ash::smb_client
