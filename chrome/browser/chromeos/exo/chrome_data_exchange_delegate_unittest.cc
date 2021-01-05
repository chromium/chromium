// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/exo/chrome_data_exchange_delegate.h"

#include "ash/public/cpp/app_types.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/file_info/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

namespace chromeos {

namespace {

std::vector<uint8_t> Data(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

void Capture(std::string* result, scoped_refptr<base::RefCountedMemory> data) {
  *result = std::string(data->front_as<char>(), data->size());
}

void CaptureUTF16(std::string* result,
                  scoped_refptr<base::RefCountedMemory> data) {
  base::UTF16ToUTF8(data->front_as<base::char16>(), data->size() / 2, result);
}

}  // namespace

class ChromeDataExchangeDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
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

    // Register my files and crostini.
    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    myfiles_mount_name_ =
        file_manager::util::GetDownloadsMountPointName(profile_.get());
    myfiles_dir_ =
        file_manager::util::GetMyFilesFolderForProfile(profile_.get());
    mount_points_->RegisterFileSystem(
        myfiles_mount_name_, storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), myfiles_dir_);
    crostini_mount_name_ =
        file_manager::util::GetCrostiniMountPointName(profile_.get());
    crostini_dir_ =
        file_manager::util::GetCrostiniMountDirectory(profile_.get());
    mount_points_->RegisterFileSystem(
        crostini_mount_name_, storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), crostini_dir_);

    // ChromeDataExchangeDelegate always checks app type in
    // window->GetToplevelWindow(), so we must create a parent window with
    // delegate and app type set, but use the child window in tests. Arc:
    arc_toplevel_ = aura::test::CreateTestWindowWithDelegate(
        &delegate_, 0, gfx::Rect(), nullptr);
    arc_toplevel_->SetProperty(aura::client::kAppType,
                               static_cast<int>(ash::AppType::ARC_APP));
    ASSERT_TRUE(ash::IsArcWindow(arc_toplevel_));
    arc_window_ =
        aura::test::CreateTestWindowWithBounds(gfx::Rect(), arc_toplevel_);
    ASSERT_TRUE(ash::IsArcWindow(arc_window_->GetToplevelWindow()));

    // Crostini:
    crostini_toplevel_ = aura::test::CreateTestWindowWithDelegate(
        &delegate_, 0, gfx::Rect(), nullptr);
    crostini_toplevel_->SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::CROSTINI_APP));
    ASSERT_TRUE(crostini::IsCrostiniWindow(crostini_toplevel_));
    crostini_window_ =
        aura::test::CreateTestWindowWithBounds(gfx::Rect(), crostini_toplevel_);
    ASSERT_TRUE(
        crostini::IsCrostiniWindow(crostini_window_->GetToplevelWindow()));

    // Plugin VM:
    plugin_vm_toplevel_ = aura::test::CreateTestWindowWithDelegate(
        &delegate_, 0, gfx::Rect(), nullptr);
    exo::SetShellApplicationId(plugin_vm_toplevel_,
                               "org.chromium.plugin_vm_ui");
    ASSERT_TRUE(plugin_vm::IsPluginVmAppWindow(plugin_vm_toplevel_));
    plugin_vm_window_ = aura::test::CreateTestWindowWithBounds(
        gfx::Rect(), plugin_vm_toplevel_);
    ASSERT_TRUE(
        plugin_vm::IsPluginVmAppWindow(plugin_vm_window_->GetToplevelWindow()));

    // DBus seneschal client.
    fake_seneschal_client_ = static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
    ASSERT_TRUE(fake_seneschal_client_);
  }

  void TearDown() override {
    mount_points_->RevokeAllFileSystems();
    test_helper_.reset();
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<crostini::CrostiniTestHelper> test_helper_;

  aura::test::TestWindowDelegate delegate_;
  aura::Window* arc_toplevel_;
  aura::Window* arc_window_;
  aura::Window* crostini_toplevel_;
  aura::Window* crostini_window_;
  aura::Window* plugin_vm_toplevel_;
  aura::Window* plugin_vm_window_;

  storage::ExternalMountPoints* mount_points_;
  std::string myfiles_mount_name_;
  base::FilePath myfiles_dir_;
  std::string crostini_mount_name_;
  base::FilePath crostini_dir_;

  chromeos::FakeSeneschalClient* fake_seneschal_client_ = nullptr;
};

TEST_F(ChromeDataExchangeDelegateTest, GetFilenames) {
  ChromeDataExchangeDelegate data_exchange_delegate;

  // Multiple lines should be parsed.
  // Arc should not translate paths.
  std::vector<ui::FileInfo> files = data_exchange_delegate.GetFilenames(
      arc_window_, Data("\n\tfile:///file1\t\r\n#ignore\r\nfile:///file2\r\n"));
  EXPECT_EQ(2, files.size());
  EXPECT_EQ("/file1", files[0].path.value());
  EXPECT_EQ("", files[0].display_name.value());
  EXPECT_EQ("/file2", files[1].path.value());
  EXPECT_EQ("", files[1].display_name.value());

  // Crostini shared paths should be mapped.
  files = data_exchange_delegate.GetFilenames(
      crostini_window_, Data("file:///mnt/chromeos/MyFiles/file"));
  EXPECT_EQ(myfiles_dir_.Append("file"), files[0].path);

  // Crostini homedir should be mapped.
  files = data_exchange_delegate.GetFilenames(
      crostini_window_, Data("file:///home/testuser/file"));
  EXPECT_EQ(crostini_dir_.Append("file"), files[0].path);

  // Crostini internal paths should be mapped.
  files = data_exchange_delegate.GetFilenames(crostini_window_,
                                              Data("file:///etc/hosts"));
  EXPECT_EQ("vmfile:termina:/etc/hosts", files[0].path.value());

  // Plugin VM shared paths should be mapped.
  files = data_exchange_delegate.GetFilenames(
      plugin_vm_window_, Data("file://ChromeOS/MyFiles/file"));
  EXPECT_EQ(myfiles_dir_.Append("file"), files[0].path);

  // Plugin VM internal paths should be mapped.
  files = data_exchange_delegate.GetFilenames(
      plugin_vm_window_, Data("file:///C:/WINDOWS/notepad.exe"));
  EXPECT_EQ("vmfile:PvmDefault:/C:/WINDOWS/notepad.exe", files[0].path.value());
}

TEST_F(ChromeDataExchangeDelegateTest, GetMimeTypeForUriList) {
  ChromeDataExchangeDelegate data_exchange_delegate;
  EXPECT_EQ("application/x-arc-uri-list",
            data_exchange_delegate.GetMimeTypeForUriList(arc_window_));
  EXPECT_EQ("text/uri-list",
            data_exchange_delegate.GetMimeTypeForUriList(crostini_window_));
  EXPECT_EQ("text/uri-list",
            data_exchange_delegate.GetMimeTypeForUriList(plugin_vm_window_));
}

TEST_F(ChromeDataExchangeDelegateTest, SendFileInfoConvertPaths) {
  ChromeDataExchangeDelegate data_exchange_delegate;
  ui::FileInfo file1(myfiles_dir_.Append("file1"), base::FilePath());
  ui::FileInfo file2(myfiles_dir_.Append("file2"), base::FilePath());
  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile());
  guest_os_share_path->RegisterSharedPath(plugin_vm::kPluginVmName,
                                          myfiles_dir_);

  // Arc should convert path to UTF16 URL.
  std::string data;
  data_exchange_delegate.SendFileInfo(arc_window_, {file1},
                                      base::BindOnce(&CaptureUTF16, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(
      "content://org.chromium.arc.volumeprovider/"
      "0000000000000000000000000000CAFEF00D2019/file1",
      data);

  // Arc should join lines with CRLF.
  data_exchange_delegate.SendFileInfo(arc_window_, {file1, file2},
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
  data_exchange_delegate.SendFileInfo(crostini_window_, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/file1", data);

  // Crostini should join lines with CRLF.
  data_exchange_delegate.SendFileInfo(crostini_window_, {file1, file2},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(
      "file:///mnt/chromeos/MyFiles/file1"
      "\r\n"
      "file:///mnt/chromeos/MyFiles/file2",
      data);

  // Plugin VM should convert path to inside VM.
  data_exchange_delegate.SendFileInfo(plugin_vm_window_, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file://ChromeOS/MyFiles/file1", data);

  // Crostini should handle vmfile:termina:/etc/hosts.
  file1.path = base::FilePath("vmfile:termina:/etc/hosts");
  data_exchange_delegate.SendFileInfo(crostini_window_, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///etc/hosts", data);

  // Crostini should ignore vmfile:PvmDefault:C:/WINDOWS/notepad.exe.
  file1.path = base::FilePath("vmfile:PvmDefault:C:/WINDOWS/notepad.exe");
  data_exchange_delegate.SendFileInfo(crostini_window_, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);

  // Plugin VM should handle vmfile:PvmDefault:C:/WINDOWS/notepad.exe.
  file1.path = base::FilePath("vmfile:PvmDefault:C:/WINDOWS/notepad.exe");
  data_exchange_delegate.SendFileInfo(plugin_vm_window_, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///C:/WINDOWS/notepad.exe", data);

  // Crostini should handle vmfile:termina:/etc/hosts.
  file1.path = base::FilePath("vmfile:termina:/etc/hosts");
  data_exchange_delegate.SendFileInfo(plugin_vm_window_, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);
}

TEST_F(ChromeDataExchangeDelegateTest, SendFileInfoSharePathsCrostini) {
  ChromeDataExchangeDelegate data_exchange_delegate;

  // A path which is already shared should not be shared again.
  base::FilePath shared_path = myfiles_dir_.Append("shared");
  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile());
  guest_os_share_path->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                          shared_path);
  ui::FileInfo file(shared_path, base::FilePath());
  EXPECT_FALSE(fake_seneschal_client_->share_path_called());
  std::string data;
  data_exchange_delegate.SendFileInfo(crostini_window_, {file},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/shared", data);
  EXPECT_FALSE(fake_seneschal_client_->share_path_called());

  // A path which is not already shared should be shared.
  file = ui::FileInfo(myfiles_dir_.Append("file"), base::FilePath());
  data_exchange_delegate.SendFileInfo(crostini_window_, {file},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/file", data);
  EXPECT_TRUE(fake_seneschal_client_->share_path_called());
}

TEST_F(ChromeDataExchangeDelegateTest, SendFileInfoSharePathsPluginVm) {
  ChromeDataExchangeDelegate data_exchange_delegate;

  // Plugin VM should send empty data and not share path if not already shared.
  ui::FileInfo file(myfiles_dir_.Append("file"), base::FilePath());
  std::string data;
  data_exchange_delegate.SendFileInfo(plugin_vm_window_, {file},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);
  EXPECT_FALSE(fake_seneschal_client_->share_path_called());
}

TEST_F(ChromeDataExchangeDelegateTest, HasUrlsInPickle) {
  ChromeDataExchangeDelegate data_exchange_delegate;

  // Pickle empty.
  base::Pickle empty;
  EXPECT_EQ(false, data_exchange_delegate.HasUrlsInPickle(empty));

  // Invalid FileInfo.url.
  base::Pickle invalid;
  content::DropData::FileSystemFileInfo file_info;
  content::DropData::FileSystemFileInfo::WriteFileSystemFilesToPickle(
      {file_info}, &invalid);
  EXPECT_EQ(false, data_exchange_delegate.HasUrlsInPickle(invalid));

  // Valid FileInfo.url.
  base::Pickle valid;
  storage::FileSystemURL url = mount_points_->CreateExternalFileSystemURL(
      url::Origin::Create(GURL("http://example.com")), myfiles_mount_name_,
      base::FilePath("path"));
  file_info.url = url.ToGURL();
  content::DropData::FileSystemFileInfo::WriteFileSystemFilesToPickle(
      {file_info}, &valid);
  EXPECT_EQ(true, data_exchange_delegate.HasUrlsInPickle(valid));
}

TEST_F(ChromeDataExchangeDelegateTest, GetDataTransferEndpointType) {
  ChromeDataExchangeDelegate data_exchange_delegate;

  ui::OSExchangeData os_exchange_data;

  EXPECT_EQ(ui::EndpointType::kArc,
            data_exchange_delegate.GetDataTransferEndpointType(arc_window_));

  EXPECT_EQ(
      ui::EndpointType::kCrostini,
      data_exchange_delegate.GetDataTransferEndpointType(crostini_window_));

  EXPECT_EQ(
      ui::EndpointType::kPluginVm,
      data_exchange_delegate.GetDataTransferEndpointType(plugin_vm_window_));
}

TEST_F(ChromeDataExchangeDelegateTest, SetExchangeDataSource) {
  ChromeDataExchangeDelegate data_exchange_delegate;

  ui::OSExchangeData os_exchange_data;

  data_exchange_delegate.SetSourceOnOSExchangeData(arc_window_,
                                                   &os_exchange_data);
  EXPECT_TRUE(os_exchange_data.GetSource());
  EXPECT_EQ(ui::EndpointType::kArc, os_exchange_data.GetSource()->type());

  data_exchange_delegate.SetSourceOnOSExchangeData(crostini_window_,
                                                   &os_exchange_data);
  EXPECT_TRUE(os_exchange_data.GetSource());
  EXPECT_EQ(ui::EndpointType::kCrostini, os_exchange_data.GetSource()->type());
}

}  // namespace chromeos
