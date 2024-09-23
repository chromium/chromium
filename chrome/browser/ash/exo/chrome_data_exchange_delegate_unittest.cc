// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"

#include "ash/public/cpp/app_types_util.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/common/drop_data.h"
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
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class ChromeDataExchangeDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile_.get());

    // Register MyFiles and Crostini.
    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    // For example, "Downloads-test%40example.com-hash"
    myfiles_mount_name_ =
        file_manager::util::GetDownloadsMountPointName(profile_.get());
    // For example, "$HOME/Downloads"
    myfiles_dir_ =
        file_manager::util::GetMyFilesFolderForProfile(profile_.get());
    mount_points_->RegisterFileSystem(
        myfiles_mount_name_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), myfiles_dir_);
  }

  void TearDown() override {
    mount_points_->RevokeAllFileSystems();
    test_helper_.reset();
    profile_.reset();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<crostini::CrostiniTestHelper> test_helper_;
  aura::test::TestWindowDelegate delegate_;
  raw_ptr<storage::ExternalMountPoints> mount_points_;
  std::string myfiles_mount_name_;
  base::FilePath myfiles_dir_;
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
  arc_toplevel->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  ASSERT_TRUE(IsArcWindow(arc_toplevel));
  aura::Window* arc_window =
      aura::test::CreateTestWindowWithBounds(gfx::Rect(), arc_toplevel);
  ASSERT_TRUE(IsArcWindow(arc_window->GetToplevelWindow()));

  // Crostini:
  aura::Window* crostini_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate_, 0, gfx::Rect(), &container_window);
  crostini_toplevel->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::CROSTINI_APP);
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

}  // namespace ash
