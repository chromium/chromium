// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_TEST_BASE_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_TEST_BASE_H_

#include <stddef.h>

#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/smbfs/smbfs_mounter.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;

namespace ash::smb_client {

namespace {

inline constexpr char kSharePath[] = "\\\\server\\foobar";
inline constexpr char kMountPath[] = "/share/mount/path";
inline constexpr char kDisplayName[] = "My Share";

}  // namespace

class MockSmbFsMounter : public smbfs::SmbFsMounter {
 public:
  MockSmbFsMounter();
  ~MockSmbFsMounter() override;

  MOCK_METHOD(void,
              Mount,
              (smbfs::SmbFsMounter::DoneCallback callback),
              (override));
};

class MockSmbFsImpl : public smbfs::mojom::SmbFs {
 public:
  explicit MockSmbFsImpl(mojo::PendingReceiver<smbfs::mojom::SmbFs> pending);
  ~MockSmbFsImpl() override;

  MockSmbFsImpl(const MockSmbFsImpl&) = delete;
  MockSmbFsImpl& operator=(const MockSmbFsImpl&) = delete;

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

class SmbServiceBaseTest : public testing::Test {
 public:
  SmbServiceBaseTest();

  SmbServiceBaseTest(const SmbServiceBaseTest&) = delete;
  SmbServiceBaseTest& operator=(const SmbServiceBaseTest&) = delete;
  ~SmbServiceBaseTest() override;

 protected:
  // Mojo endpoints owned by the smbfs instance.
  struct TestSmbFsInstance {
    explicit TestSmbFsInstance(
        mojo::PendingReceiver<smbfs::mojom::SmbFs> pending);
    ~TestSmbFsInstance();

    MockSmbFsImpl mock_smbfs;
    mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;
  };

  // Create smb service.
  void CreateService(TestingProfile* profile);

  // Checks that the correct result status is returned when an SMB attempts
  // mount with an invalid URL.
  void ExpectInvalidUrl(const std::string& url);

  // Checks that the correct result status is returned when an SMB attempts
  // mount with an invalid SSO URL.
  void ExpectInvalidSsoUrl(const std::string& url);

  void WaitForSetupComplete();

  std::unique_ptr<disks::MountPoint> MakeMountPoint(const base::FilePath& path);

  // Helper function for creating a basic smbfs mount with an empty
  // username/password.
  std::unique_ptr<TestSmbFsInstance> MountBasicShare(
      const std::string& share_path,
      const std::string& mount_path,
      SmbService::MountResponse callback);

  std::unique_ptr<SmbService> smb_service;

  TestingProfile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
  raw_ptr<disks::FakeDiskMountManager, DanglingUntriaged> disk_mount_manager_ =
      nullptr;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_TEST_BASE_H_
