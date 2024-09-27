// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_security_delegate.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_security_delegate.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

std::vector<uint8_t> Data(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

void Capture(std::string* result, scoped_refptr<base::RefCountedMemory> data) {
  *result = std::string(base::as_string_view(*data));
}

void CaptureUTF16(std::string* result,
                  scoped_refptr<base::RefCountedMemory> data) {
  base::span<const uint8_t> bytes = *data;
  std::u16string str(bytes.size() / 2u, u'\0');
  base::as_writable_byte_span(str).copy_from(bytes);
  *result = base::UTF16ToUTF8(str);
}

}  // namespace

class ChromeSecurityDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    ChunneldClient::InitializeFake();
    CiceroneClient::InitializeFake();
    ConciergeClient::InitializeFake();
    SeneschalClient::InitializeFake();

    profile_ = std::make_unique<TestingProfile>();
    test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile_.get());

    // Setup CrostiniManager for testing.
    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(profile_.get());
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
    crostini_manager->AddRunningContainerForTesting(
        crostini::kCrostiniDefaultVmName,
        crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                                "testuser", "/home/testuser",
                                "PLACEHOLDER_IP"));

    // Register MyFiles and Crostini.
    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    // Downloads-test%40example.com-hash
    myfiles_mount_name_ =
        file_manager::util::GetDownloadsMountPointName(profile_.get());
    // $HOME/Downloads
    myfiles_dir_ =
        file_manager::util::GetMyFilesFolderForProfile(profile_.get());
    mount_points_->RegisterFileSystem(
        myfiles_mount_name_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), myfiles_dir_);
    // crostini_test_termina_penguin
    crostini_mount_name_ =
        file_manager::util::GetCrostiniMountPointName(profile_.get());
    // /media/fuse/crostini_test_termina_penguin
    crostini_dir_ =
        file_manager::util::GetCrostiniMountDirectory(profile_.get());
    mount_points_->RegisterFileSystem(
        crostini_mount_name_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), crostini_dir_);
  }

  void TearDown() override {
    mount_points_->RevokeAllFileSystems();
    test_helper_.reset();
    profile_.reset();
    SeneschalClient::Shutdown();
    ConciergeClient::Shutdown();
    CiceroneClient::Shutdown();
    ChunneldClient::Shutdown();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<crostini::CrostiniTestHelper> test_helper_;

  raw_ptr<storage::ExternalMountPoints> mount_points_;
  std::string myfiles_mount_name_;
  base::FilePath myfiles_dir_;
  std::string crostini_mount_name_;
  base::FilePath crostini_dir_;
};

TEST_F(ChromeSecurityDelegateTest, CanLockPointer) {
  auto security_delegate = std::make_unique<ChromeSecurityDelegate>();
  aura::Window container_window(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  container_window.Init(ui::LAYER_NOT_DRAWN);
  aura::test::TestWindowDelegate delegate;

  // CanLockPointer should be allowed for arc and lacros, but not others.
  std::unique_ptr<aura::Window> arc_toplevel(
      aura::test::CreateTestWindowWithDelegate(&delegate, 0, gfx::Rect(),
                                               &container_window));
  arc_toplevel->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  EXPECT_TRUE(security_delegate->CanLockPointer(arc_toplevel.get()));

  std::unique_ptr<aura::Window> lacros_toplevel(
      aura::test::CreateTestWindowWithDelegate(&delegate, 0, gfx::Rect(),
                                               &container_window));
  lacros_toplevel->SetProperty(chromeos::kAppTypeKey,
                               chromeos::AppType::LACROS);
  EXPECT_TRUE(security_delegate->CanLockPointer(lacros_toplevel.get()));

  std::unique_ptr<aura::Window> crostini_toplevel(
      aura::test::CreateTestWindowWithDelegate(&delegate, 0, gfx::Rect(),
                                               &container_window));
  crostini_toplevel->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::CROSTINI_APP);
  EXPECT_FALSE(security_delegate->CanLockPointer(crostini_toplevel.get()));
}

TEST_F(ChromeSecurityDelegateTest, GetFilenames) {
  ChromeSecurityDelegate security_delegate;
  base::FilePath shared_path = myfiles_dir_.Append("shared");
  auto* guest_os_share_path =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile());
  guest_os_share_path->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                          shared_path);
  guest_os_share_path->RegisterSharedPath(plugin_vm::kPluginVmName,
                                          shared_path);
  guest_os_share_path->RegisterSharedPath(bruschetta::kBruschettaVmName,
                                          shared_path);

  // Multiple lines should be parsed.
  // Arc should not translate paths.
  std::vector<ui::FileInfo> files = security_delegate.GetFilenames(
      ui::EndpointType::kArc,
      Data("\n\tfile:///file1\t\r\n#ignore\r\nfile:///file2\r\n"));
  EXPECT_EQ(2u, files.size());
  EXPECT_EQ("/file1", files[0].path.value());
  EXPECT_EQ("", files[0].display_name.value());
  EXPECT_EQ("/file2", files[1].path.value());
  EXPECT_EQ("", files[1].display_name.value());

  // Crostini shared paths should be mapped.
  guest_os::GuestOsSecurityDelegate crostini_security_delegate("termina");
  files = crostini_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/chromeos/MyFiles/shared/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file"), files[0].path);

  // Crostini homedir should be mapped.
  files = crostini_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("file:///home/testuser/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(crostini_dir_.Append("file"), files[0].path);

  // Crostini internal paths should be mapped.
  files = crostini_security_delegate.GetFilenames(ui::EndpointType::kCrostini,
                                                  Data("file:///etc/hosts"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:termina:/etc/hosts", files[0].path.value());

  // Unshared paths should fail.
  files = crostini_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/chromeos/MyFiles/unshared/file"));
  EXPECT_EQ(0u, files.size());
  files = crostini_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/chromeos/MyFiles/shared/file1\r\n"
           "file:///mnt/chromeos/MyFiles/unshared/file2"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file1"), files[0].path);

  // file:/path should fail.
  files = crostini_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("file:/mnt/chromeos/MyFiles/file"));
  EXPECT_EQ(0u, files.size());

  // file:path should fail.
  files = crostini_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("file:mnt/chromeos/MyFiles/file"));
  EXPECT_EQ(0u, files.size());

  // file:// should fail.
  files = crostini_security_delegate.GetFilenames(ui::EndpointType::kCrostini,
                                                  Data("file://"));
  EXPECT_EQ(0u, files.size());

  // file:/// maps to internal root.
  files = crostini_security_delegate.GetFilenames(ui::EndpointType::kCrostini,
                                                  Data("file:///"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:termina:/", files[0].path.value());

  // /path should fail.
  files = crostini_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("/mnt/chromeos/MyFiles/file"));
  EXPECT_EQ(0u, files.size());

  // Plugin VM shared paths should be mapped.
  files = security_delegate.GetFilenames(
      ui::EndpointType::kPluginVm, Data("file://ChromeOS/MyFiles/shared/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file"), files[0].path);

  // Plugin VM internal paths should be mapped.
  files = security_delegate.GetFilenames(
      ui::EndpointType::kPluginVm, Data("file:///C:/WINDOWS/notepad.exe"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:PvmDefault:C:/WINDOWS/notepad.exe", files[0].path.value());

  // Unshared paths should fail.
  files = security_delegate.GetFilenames(
      ui::EndpointType::kPluginVm,
      Data("file://ChromeOS/MyFiles/unshared/file"));
  EXPECT_EQ(0u, files.size());
  files = security_delegate.GetFilenames(
      ui::EndpointType::kPluginVm,
      Data("file://ChromeOS/MyFiles/shared/file1\r\n"
           "file://ChromeOS/MyFiles/unshared/file2"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file1"), files[0].path);

  // Bruschetta shared paths should be mapped.
  guest_os::GuestOsSecurityDelegate bru_security_delegate("bru");
  files = bru_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/shared/MyFiles/shared/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file"), files[0].path);

  // Bruschetta homedir is mapped as an internal path.
  files = bru_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("file:///home/testuser/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:bru:/home/testuser/file", files[0].path.value());

  // Bruschetta internal paths should be mapped.
  files = bru_security_delegate.GetFilenames(ui::EndpointType::kCrostini,
                                             Data("file:///etc/hosts"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:bru:/etc/hosts", files[0].path.value());

  // Unshared paths should fail.
  files = bru_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/shared/MyFiles/unshared/file"));
  EXPECT_EQ(0u, files.size());
  files = bru_security_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/shared/MyFiles/shared/file1\r\n"
           "file:///mnt/shared/MyFiles/unshared/file2"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file1"), files[0].path);
}

TEST_F(ChromeSecurityDelegateTest, SendFileInfoConvertPaths) {
  ChromeSecurityDelegate security_delegate;
  ui::FileInfo file1(myfiles_dir_.Append("file1"), base::FilePath());
  ui::FileInfo file2(myfiles_dir_.Append("file2"), base::FilePath());
  auto* guest_os_share_path =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile());
  guest_os_share_path->RegisterSharedPath(plugin_vm::kPluginVmName,
                                          myfiles_dir_);

  // Arc should convert path to UTF16 URL.
  std::string data;
  security_delegate.SendFileInfo(ui::EndpointType::kArc, {file1},
                                 base::BindOnce(&CaptureUTF16, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(
      "content://org.chromium.arc.volumeprovider/"
      "0000000000000000000000000000CAFEF00D2019/file1",
      data);

  // Arc should join lines with CRLF.
  security_delegate.SendFileInfo(ui::EndpointType::kArc, {file1, file2},
                                 base::BindOnce(&CaptureUTF16, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(
      "content://org.chromium.arc.volumeprovider/"
      "0000000000000000000000000000CAFEF00D2019/file1"
      "\r\n"
      "content://org.chromium.arc.volumeprovider/"
      "0000000000000000000000000000CAFEF00D2019/file2",
      data);

  // Crostini should convert path to inside VM, and share the path.
  guest_os::GuestOsSecurityDelegate crostini_security_delegate("termina");
  crostini_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                          base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/file1", data);

  // Crostini should join lines with CRLF.
  crostini_security_delegate.SendFileInfo(ui::EndpointType::kCrostini,
                                          {file1, file2},
                                          base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(
      "file:///mnt/chromeos/MyFiles/file1"
      "\r\n"
      "file:///mnt/chromeos/MyFiles/file2",
      data);

  // Plugin VM should convert path to inside VM.
  security_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file1},
                                 base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file://ChromeOS/MyFiles/file1", data);

  // Bruschetta should convert path to inside VM, and share the path.
  guest_os::GuestOsSecurityDelegate bru_security_delegate("bru");
  bru_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                     base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/shared/MyFiles/file1", data);

  // Crostini should handle vmfile:termina:/etc/hosts.
  file1.path = base::FilePath("vmfile:termina:/etc/hosts");
  crostini_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                          base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///etc/hosts", data);

  // Crostini should ignore vmfile:PvmDefault:C:/WINDOWS/notepad.exe.
  file1.path = base::FilePath("vmfile:PvmDefault:C:/WINDOWS/notepad.exe");
  crostini_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                          base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);

  // Plugin VM should handle vmfile:PvmDefault:C:/WINDOWS/notepad.exe.
  file1.path = base::FilePath("vmfile:PvmDefault:C:/WINDOWS/notepad.exe");
  security_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file1},
                                 base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///C:/WINDOWS/notepad.exe", data);

  // Crostini should handle vmfile:termina:/etc/hosts.
  file1.path = base::FilePath("vmfile:termina:/etc/hosts");
  security_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file1},
                                 base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);

  // Bruschetta should handle vmfile:bru:/etc/hosts.
  file1.path = base::FilePath("vmfile:bru:/etc/hosts");
  bru_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                     base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///etc/hosts", data);

  // Bruschetta should ignore vmfile:termina:/etc/hosts.
  file1.path = base::FilePath("vmfile:termina:/etc/hosts");
  bru_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                     base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);
}

TEST_F(ChromeSecurityDelegateTest, SendFileInfoSharePathsCrostini) {
  guest_os::GuestOsSecurityDelegate crostini_security_delegate("termina");

  // A path which is already shared should not be shared again.
  base::FilePath shared_path = myfiles_dir_.Append("shared");
  auto* guest_os_share_path =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile());
  guest_os_share_path->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                          shared_path);
  ui::FileInfo file(shared_path, base::FilePath());
  EXPECT_FALSE(FakeSeneschalClient::Get()->share_path_called());
  std::string data;
  crostini_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file},
                                          base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/shared", data);
  EXPECT_FALSE(FakeSeneschalClient::Get()->share_path_called());

  // A path which is not already shared should be shared.
  file = ui::FileInfo(myfiles_dir_.Append("file"), base::FilePath());
  crostini_security_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file},
                                          base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/file", data);
  EXPECT_TRUE(FakeSeneschalClient::Get()->share_path_called());
}

TEST_F(ChromeSecurityDelegateTest, SendFileInfoSharePathsPluginVm) {
  ChromeSecurityDelegate security_delegate;

  // Plugin VM should send empty data and not share path if not already shared.
  ui::FileInfo file(myfiles_dir_.Append("file"), base::FilePath());
  std::string data;
  security_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file},
                                 base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);
  EXPECT_FALSE(FakeSeneschalClient::Get()->share_path_called());
}
}  // namespace ash
