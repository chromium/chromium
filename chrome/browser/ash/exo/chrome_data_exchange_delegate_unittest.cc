// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"

#include "ash/public/cpp/app_types_util.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/account_id/account_id.h"
#include "components/exo/shell_surface_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager_impl.h"
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
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState()));
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@test", GaiaId("12345"));
    ASSERT_TRUE(user_manager::TestHelper(user_manager_.Get())
                    .AddRegularUser(account_id));
    user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));

    {
      ash::ScopedAccountIdAnnotator annotator(
          profile_manager_->profile_manager(), account_id);
      profile_ =
          profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    }

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
    profile_ = nullptr;
    profile_manager_.reset();
    user_manager_.Reset();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
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
  aura::Window* arc_toplevel =
      aura::test::CreateTestWindow(
          {.delegate = &delegate_, .parent = &container_window})
          .release();
  arc_toplevel->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  ASSERT_TRUE(IsArcWindow(arc_toplevel));
  aura::Window* arc_window =
      aura::test::CreateTestWindow({.parent = arc_toplevel}).release();
  ASSERT_TRUE(IsArcWindow(arc_window->GetToplevelWindow()));

  // Crostini:
  aura::Window* crostini_toplevel =
      aura::test::CreateTestWindow(
          {.delegate = &delegate_, .parent = &container_window})
          .release();
  crostini_toplevel->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::CROSTINI_APP);
  ASSERT_TRUE(crostini::IsCrostiniWindow(crostini_toplevel));
  aura::Window* crostini_window =
      aura::test::CreateTestWindow({.parent = crostini_toplevel}).release();
  ASSERT_TRUE(crostini::IsCrostiniWindow(crostini_window->GetToplevelWindow()));

  // Plugin VM:
  aura::Window* plugin_vm_toplevel =
      aura::test::CreateTestWindow(
          {.delegate = &delegate_, .parent = &container_window})
          .release();
  exo::SetShellApplicationId(plugin_vm_toplevel, "org.chromium.plugin_vm_ui");
  ASSERT_TRUE(plugin_vm::IsPluginVmAppWindow(plugin_vm_toplevel));
  aura::Window* plugin_vm_window =
      aura::test::CreateTestWindow({.parent = plugin_vm_toplevel}).release();
  ASSERT_TRUE(
      plugin_vm::IsPluginVmAppWindow(plugin_vm_window->GetToplevelWindow()));

  ChromeDataExchangeDelegate data_exchange_delegate;

  ui::OSExchangeData os_exchange_data;

  EXPECT_EQ(ui::EndpointType::kArc,
            data_exchange_delegate.GetDataTransferEndpointType(arc_window));

  EXPECT_EQ(
      ui::EndpointType::kCrostini,
      data_exchange_delegate.GetDataTransferEndpointType(crostini_window));

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
