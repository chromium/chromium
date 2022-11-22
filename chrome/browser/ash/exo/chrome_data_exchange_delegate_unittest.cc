// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/constants/app_types.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/rect.h"
#include "url/url_constants.h"

namespace ash {

namespace {

std::vector<uint8_t> Data(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

void Capture(std::string* result, scoped_refptr<base::RefCountedMemory> data) {
  *result = std::string(data->front_as<char>(), data->size());
}

void CaptureUTF16(std::string* result,
                  scoped_refptr<base::RefCountedMemory> data) {
  base::UTF16ToUTF8(data->front_as<char16_t>(), data->size() / 2, result);
}

}  // namespace

class ChromeDataExchangeDelegateTest : public testing::Test {
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

    // Register my files and crostini.
    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    myfiles_mount_name_ =
        file_manager::util::GetDownloadsMountPointName(profile_.get());
    myfiles_dir_ =
        file_manager::util::GetMyFilesFolderForProfile(profile_.get());
    mount_points_->RegisterFileSystem(
        myfiles_mount_name_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), myfiles_dir_);
    crostini_mount_name_ =
        file_manager::util::GetCrostiniMountPointName(profile_.get());
    crostini_dir_ =
        file_manager::util::GetCrostiniMountDirectory(profile_.get());
    mount_points_->RegisterFileSystem(
        crostini_mount_name_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), crostini_dir_);

    // DBus seneschal client.
    fake_seneschal_client_ = FakeSeneschalClient::Get();
    ASSERT_TRUE(fake_seneschal_client_);
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

  aura::test::TestWindowDelegate delegate_;

  storage::ExternalMountPoints* mount_points_;
  std::string myfiles_mount_name_;
  base::FilePath myfiles_dir_;
  std::string crostini_mount_name_;
  base::FilePath crostini_dir_;

  FakeSeneschalClient* fake_seneschal_client_ = nullptr;
};

TEST_F(ChromeDataExchangeDelegateTest, GetDataTransferEndpointType) {
  // Create container window as the parent for other windows.
  aura::Window container_window(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  container_window.Init(ui::LAYER_NOT_DRAWN);

  // ChromeDataExchangeDelegate always checks app type in
  // window->GetToplevelWindow(), so we must create a parent window with
  // delegate and app type set, but use the child window in tests. Arc:
  aura::Window* arc_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate_, 0, gfx::Rect(), &container_window);
  arc_toplevel->SetProperty(aura::client::kAppType,
                            static_cast<int>(AppType::ARC_APP));
  ASSERT_TRUE(IsArcWindow(arc_toplevel));
  aura::Window* arc_window =
      aura::test::CreateTestWindowWithBounds(gfx::Rect(), arc_toplevel);
  ASSERT_TRUE(IsArcWindow(arc_window->GetToplevelWindow()));

  // Crostini:
  aura::Window* crostini_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate_, 0, gfx::Rect(), &container_window);
  crostini_toplevel->SetProperty(aura::client::kAppType,
                                 static_cast<int>(AppType::CROSTINI_APP));
  ASSERT_TRUE(crostini::IsCrostiniWindow(crostini_toplevel));
  aura::Window* crostini_window =
      aura::test::CreateTestWindowWithBounds(gfx::Rect(), crostini_toplevel);
  ASSERT_TRUE(crostini::IsCrostiniWindow(crostini_window->GetToplevelWindow()));

  // Lacros:
  aura::Window* lacros_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate_, 0, gfx::Rect(), &container_window);
  exo::SetShellApplicationId(lacros_toplevel, "org.chromium.lacros.");
  ASSERT_TRUE(crosapi::browser_util::IsLacrosWindow(lacros_toplevel));
  aura::Window* lacros_window =
      aura::test::CreateTestWindowWithBounds(gfx::Rect(), lacros_toplevel);
  ASSERT_TRUE(crosapi::browser_util::IsLacrosWindow(
      lacros_window->GetToplevelWindow()));

  // Plugin VM:
  aura::Window* plugin_vm_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate_, 0, gfx::Rect(), &container_window);
  exo::SetShellApplicationId(plugin_vm_toplevel, "org.chromium.plugin_vm_ui");
  ASSERT_TRUE(plugin_vm::IsPluginVmAppWindow(plugin_vm_toplevel));
  aura::Window* plugin_vm_window =
      aura::test::CreateTestWindowWithBounds(gfx::Rect(), plugin_vm_toplevel);
  ASSERT_TRUE(
      plugin_vm::IsPluginVmAppWindow(plugin_vm_window->GetToplevelWindow()));

  ChromeDataExchangeDelegate data_exchange_delegate;

  ui::OSExchangeData os_exchange_data;

  EXPECT_EQ(ui::EndpointType::kArc,
            data_exchange_delegate.GetDataTransferEndpointType(arc_window));

  EXPECT_EQ(
      ui::EndpointType::kCrostini,
      data_exchange_delegate.GetDataTransferEndpointType(crostini_window));

  EXPECT_EQ(ui::EndpointType::kLacros,
            data_exchange_delegate.GetDataTransferEndpointType(lacros_window));

  EXPECT_EQ(
      ui::EndpointType::kPluginVm,
      data_exchange_delegate.GetDataTransferEndpointType(plugin_vm_window));
}

TEST_F(ChromeDataExchangeDelegateTest, GetFilenames) {
  ChromeDataExchangeDelegate data_exchange_delegate;
  base::FilePath shared_path = myfiles_dir_.Append("shared");
  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile());
  guest_os_share_path->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                          shared_path);
  guest_os_share_path->RegisterSharedPath(plugin_vm::kPluginVmName,
                                          shared_path);

  // Multiple lines should be parsed.
  // Arc should not translate paths.
  std::vector<ui::FileInfo> files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kArc,
      Data("\n\tfile:///file1\t\r\n#ignore\r\nfile:///file2\r\n"));
  EXPECT_EQ(2u, files.size());
  EXPECT_EQ("/file1", files[0].path.value());
  EXPECT_EQ("", files[0].display_name.value());
  EXPECT_EQ("/file2", files[1].path.value());
  EXPECT_EQ("", files[1].display_name.value());

  // Crostini shared paths should be mapped.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/chromeos/MyFiles/shared/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file"), files[0].path);

  // Crostini homedir should be mapped.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("file:///home/testuser/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(crostini_dir_.Append("file"), files[0].path);

  // Crostini internal paths should be mapped.
  files = data_exchange_delegate.GetFilenames(ui::EndpointType::kCrostini,
                                              Data("file:///etc/hosts"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:termina:/etc/hosts", files[0].path.value());

  // Unshared paths should fail.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/chromeos/MyFiles/unshared/file"));
  EXPECT_EQ(0u, files.size());
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kCrostini,
      Data("file:///mnt/chromeos/MyFiles/shared/file1\r\n"
           "file:///mnt/chromeos/MyFiles/unshared/file2"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file1"), files[0].path);

  // file:/path should fail.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("file:/mnt/chromeos/MyFiles/file"));
  EXPECT_EQ(0u, files.size());

  // file:path should fail.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("file:mnt/chromeos/MyFiles/file"));
  EXPECT_EQ(0u, files.size());

  // file:// should fail.
  files = data_exchange_delegate.GetFilenames(ui::EndpointType::kCrostini,
                                              Data("file://"));
  EXPECT_EQ(0u, files.size());

  // file:/// maps to internal root.
  files = data_exchange_delegate.GetFilenames(ui::EndpointType::kCrostini,
                                              Data("file:///"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:termina:/", files[0].path.value());

  // /path should fail.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kCrostini, Data("/mnt/chromeos/MyFiles/file"));
  EXPECT_EQ(0u, files.size());

  // Plugin VM shared paths should be mapped.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kPluginVm, Data("file://ChromeOS/MyFiles/shared/file"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file"), files[0].path);

  // Plugin VM internal paths should be mapped.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kPluginVm, Data("file:///C:/WINDOWS/notepad.exe"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ("vmfile:PvmDefault:C:/WINDOWS/notepad.exe", files[0].path.value());

  // Unshared paths should fail.
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kPluginVm,
      Data("file://ChromeOS/MyFiles/unshared/file"));
  EXPECT_EQ(0u, files.size());
  files = data_exchange_delegate.GetFilenames(
      ui::EndpointType::kPluginVm,
      Data("file://ChromeOS/MyFiles/shared/file1\r\n"
           "file://ChromeOS/MyFiles/unshared/file2"));
  EXPECT_EQ(1u, files.size());
  EXPECT_EQ(shared_path.Append("file1"), files[0].path);
}

TEST_F(ChromeDataExchangeDelegateTest, GetMimeTypeForUriList) {
  ChromeDataExchangeDelegate data_exchange_delegate;
  EXPECT_EQ(
      "application/x-arc-uri-list",
      data_exchange_delegate.GetMimeTypeForUriList(ui::EndpointType::kArc));
  EXPECT_EQ("text/uri-list", data_exchange_delegate.GetMimeTypeForUriList(
                                 ui::EndpointType::kCrostini));
  EXPECT_EQ("text/uri-list", data_exchange_delegate.GetMimeTypeForUriList(
                                 ui::EndpointType::kPluginVm));
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
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kArc, {file1},
                                      base::BindOnce(&CaptureUTF16, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(
      "content://org.chromium.arc.volumeprovider/"
      "0000000000000000000000000000CAFEF00D2019/file1",
      data);

  // Arc should join lines with CRLF.
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kArc, {file1, file2},
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
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/file1", data);

  // Crostini should join lines with CRLF.
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kCrostini,
                                      {file1, file2},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(
      "file:///mnt/chromeos/MyFiles/file1"
      "\r\n"
      "file:///mnt/chromeos/MyFiles/file2",
      data);

  // Plugin VM should convert path to inside VM.
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file://ChromeOS/MyFiles/file1", data);

  // Crostini should handle vmfile:termina:/etc/hosts.
  file1.path = base::FilePath("vmfile:termina:/etc/hosts");
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///etc/hosts", data);

  // Crostini should ignore vmfile:PvmDefault:C:/WINDOWS/notepad.exe.
  file1.path = base::FilePath("vmfile:PvmDefault:C:/WINDOWS/notepad.exe");
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", data);

  // Plugin VM should handle vmfile:PvmDefault:C:/WINDOWS/notepad.exe.
  file1.path = base::FilePath("vmfile:PvmDefault:C:/WINDOWS/notepad.exe");
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file1},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///C:/WINDOWS/notepad.exe", data);

  // Crostini should handle vmfile:termina:/etc/hosts.
  file1.path = base::FilePath("vmfile:termina:/etc/hosts");
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file1},
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
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file},
                                      base::BindOnce(&Capture, &data));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("file:///mnt/chromeos/MyFiles/shared", data);
  EXPECT_FALSE(fake_seneschal_client_->share_path_called());

  // A path which is not already shared should be shared.
  file = ui::FileInfo(myfiles_dir_.Append("file"), base::FilePath());
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kCrostini, {file},
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
  data_exchange_delegate.SendFileInfo(ui::EndpointType::kPluginVm, {file},
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
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      myfiles_mount_name_, base::FilePath("path"));
  file_info.url = url.ToGURL();
  content::DropData::FileSystemFileInfo::WriteFileSystemFilesToPickle(
      {file_info}, &valid);
  EXPECT_EQ(true, data_exchange_delegate.HasUrlsInPickle(valid));
}

TEST_F(ChromeDataExchangeDelegateTest, ParseFileSystemSources) {
  ChromeDataExchangeDelegate data_exchange_delegate;
  base::FilePath shared_path = myfiles_dir_.Append("shared");
  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile());
  guest_os_share_path->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                          shared_path);
  const GURL file_manager_url = file_manager::util::GetFileManagerURL();
  std::vector<std::string> file_names = {
      "external/Downloads-test%2540example.com-hash/shared/file1",
      "external/Downloads-test%2540example.com-hash/shared/file2",
  };
  std::vector<std::string> file_urls;
  std::transform(file_names.begin(), file_names.end(),
                 std::back_inserter(file_urls),
                 [&file_manager_url](const std::string& name) {
                   return base::StrCat({url::kFileSystemScheme, ":",
                                        file_manager_url.Resolve(name).spec()});
                 });
  std::u16string urls(base::ASCIIToUTF16(base::JoinString(file_urls, "\n")));
  base::Pickle pickle;
  ui::WriteCustomDataToPickle(
      std::unordered_map<std::u16string, std::u16string>(
          {{u"fs/tag", u"exo"}, {u"fs/sources", urls}}),
      &pickle);

  ui::DataTransferEndpoint files_app(file_manager_url.Resolve("main.html"));
  std::vector<ui::FileInfo> file_info =
      data_exchange_delegate.ParseFileSystemSources(&files_app, pickle);
  EXPECT_EQ(2u, file_info.size());
  EXPECT_EQ(shared_path.Append("file1"), file_info[0].path);
  EXPECT_EQ(shared_path.Append("file2"), file_info[1].path);
  EXPECT_EQ(base::FilePath(), file_info[0].display_name);
  EXPECT_EQ(base::FilePath(), file_info[1].display_name);

  // Should return empty if source is not FilesApp.
  ui::DataTransferEndpoint crostini(ui::EndpointType::kCrostini);
  file_info = data_exchange_delegate.ParseFileSystemSources(&crostini, pickle);
  EXPECT_TRUE(file_info.empty());
}

}  // namespace ash
