// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_uninstaller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

using CallbackFactory =
    StrictCallbackFactory<void(BorealisAppUninstaller::UninstallResult)>;

class BorealisInstallerMock : public BorealisInstaller {
 public:
  BorealisInstallerMock() = default;
  ~BorealisInstallerMock() override = default;
  MOCK_METHOD(bool, IsProcessing, (), ());
  MOCK_METHOD(void, Start, (), ());
  MOCK_METHOD(void, Cancel, (), ());
  MOCK_METHOD(void,
              Uninstall,
              (base::OnceCallback<void(BorealisUninstallResult)>),
              ());
  MOCK_METHOD(void, AddObserver, (Observer * observer), ());
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), ());
};

class BorealisLauncherMock : public BorealisAppLauncher {
 public:
  BorealisLauncherMock() = default;
  ~BorealisLauncherMock() override = default;
  MOCK_METHOD(void,
              Launch,
              (std::string app_id,
               const std::vector<std::string>& args,
               BorealisLaunchSource source,
               OnLaunchedCallback callback),
              ());
  MOCK_METHOD(void,
              Launch,
              (std::string app_id,
               BorealisLaunchSource source,
               OnLaunchedCallback callback),
              ());
};

class BorealisAppUninstallerTest : public testing::Test {
 public:
  BorealisAppUninstallerTest() = default;

 protected:
  void SetUp() override {
    CreateProfile();
    mock_installer_ =
        std::make_unique<testing::StrictMock<BorealisInstallerMock>>();
    mock_launcher_ =
        std::make_unique<testing::StrictMock<BorealisLauncherMock>>();
    BorealisServiceFake* fake_service =
        BorealisServiceFake::UseFakeForTesting(profile_.get());
    fake_service->SetInstallerForTesting(mock_installer_.get());
    fake_service->SetAppLauncherForTesting(mock_launcher_.get());
    list_ = std::make_unique<vm_tools::apps::ApplicationList>();
    list_->set_vm_name("borealis");
    list_->set_container_name("penguin");
    list_->set_vm_type(vm_tools::apps::BOREALIS);
  }

  void TearDown() override {
    profile_.reset();
    mock_installer_.reset();
    mock_launcher_.reset();
    list_.reset();
  }

  // Sets up the registry with a dummy app. Returns its app id.
  std::string AddDummyApp(vm_tools::apps::ApplicationList* list,
                          const std::string& desktop_file_id,
                          std::string exec) {
    vm_tools::apps::App* app = list->add_apps();
    app->set_desktop_file_id(desktop_file_id);
    app->set_exec(exec);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app->mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(desktop_file_id);
    app->set_no_display(false);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_.get())
        ->UpdateApplicationList(*list);
    return guest_os::GuestOsRegistryService::GenerateAppId(
        desktop_file_id, list->vm_name(), list->container_name());
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::StrictMock<BorealisInstallerMock>> mock_installer_;
  std::unique_ptr<testing::StrictMock<BorealisLauncherMock>> mock_launcher_;
  std::unique_ptr<vm_tools::apps::ApplicationList> list_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }
};

TEST_F(BorealisAppUninstallerTest, BorealisAppUninstallsBorealis) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppUninstaller::UninstallResult::kSuccess));
  BorealisAppUninstaller uninstaller = BorealisAppUninstaller(profile_.get());
  EXPECT_CALL(*mock_installer_, Uninstall(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisUninstallResult)> callback) {
            std::move(callback).Run(BorealisUninstallResult::kSuccess);
          }));
  uninstaller.Uninstall(kInstallerAppId, callback_check.BindOnce());
}

TEST_F(BorealisAppUninstallerTest, BorealisMainAppUninstallsBorealis) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppUninstaller::UninstallResult::kSuccess));
  BorealisAppUninstaller uninstaller = BorealisAppUninstaller(profile_.get());
  EXPECT_CALL(*mock_installer_, Uninstall(testing::_))
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(BorealisUninstallResult)> callback) {
            std::move(callback).Run(BorealisUninstallResult::kSuccess);
          }));
  uninstaller.Uninstall(kClientAppId, callback_check.BindOnce());
}

TEST_F(BorealisAppUninstallerTest, BorealisGameUninstalls) {
  std::string steam_id = AddDummyApp(list_.get(), "steam", "steam");
  std::string game_id =
      AddDummyApp(list_.get(), "baz.desktop", "steam://rungameid/1439770");
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppUninstaller::UninstallResult::kSuccess));
  BorealisAppUninstaller uninstaller = BorealisAppUninstaller(profile_.get());
  std::vector<std::string> v = {"steam://uninstall/1439770"};
  EXPECT_CALL(
      *mock_launcher_,
      Launch(steam_id, v, BorealisLaunchSource::kAppUninstaller, testing::_))
      .WillOnce(testing::Invoke(
          [&](std::string app_id, const std::vector<std::string>& args,
              BorealisLaunchSource source,
              BorealisAppLauncher::OnLaunchedCallback callback) {
            std::move(callback).Run(
                BorealisAppLauncher::LaunchResult::kSuccess);
          }));
  uninstaller.Uninstall(game_id, callback_check.BindOnce());
}

TEST_F(BorealisAppUninstallerTest, NonExistentAppFails) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppUninstaller::UninstallResult::kError));
  BorealisAppUninstaller uninstaller = BorealisAppUninstaller(profile_.get());
  uninstaller.Uninstall("IdontExist", callback_check.BindOnce());
}

TEST_F(BorealisAppUninstallerTest, AppWithEmptyExecFails) {
  std::string baz_id = AddDummyApp(list_.get(), "baz.desktop", "");
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppUninstaller::UninstallResult::kError));
  BorealisAppUninstaller uninstaller = BorealisAppUninstaller(profile_.get());
  uninstaller.Uninstall(baz_id, callback_check.BindOnce());
}

TEST_F(BorealisAppUninstallerTest, AppWithInvalidExecFails) {
  std::string baz_id =
      AddDummyApp(list_.get(), "test.desktop", "desktopname with no id");
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppUninstaller::UninstallResult::kError));
  BorealisAppUninstaller uninstaller = BorealisAppUninstaller(profile_.get());
  uninstaller.Uninstall(baz_id, callback_check.BindOnce());
}

}  // namespace
}  // namespace borealis
