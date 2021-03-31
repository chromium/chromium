// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_launcher.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class CallbackFactory
    : public testing::StrictMock<
          testing::MockFunction<void(BorealisAppLauncher::LaunchResult)>> {
 public:
  base::OnceCallback<void(BorealisAppLauncher::LaunchResult)> BindOnce() {
    return base::BindOnce(&CallbackFactory::Call, base::Unretained(this));
  }
};

class ScopedCiceroneFake {
 public:
  ScopedCiceroneFake() { chromeos::DBusThreadManager::Initialize(); }

  ~ScopedCiceroneFake() { chromeos::DBusThreadManager::Shutdown(); }

  chromeos::FakeCiceroneClient* Get() {
    return reinterpret_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
  }
};

class BorealisAppLauncherTest : public testing::Test {
 public:
  BorealisAppLauncherTest()
      : ctx_(BorealisContext::CreateBorealisContextForTesting(&profile_)) {
    ctx_->set_vm_name("test_vm_name");
    ctx_->set_container_name("test_container_name");
  }

 protected:
  const BorealisContext& Context() { return *ctx_; }

  chromeos::FakeCiceroneClient* Cicerone() { return cicerone_.Get(); }

  // Sets up the registry with a single app. Returns its app id.
  std::string SetDummyApp(const std::string& desktop_file_id) {
    vm_tools::apps::ApplicationList list;
    list.set_vm_name(Context().vm_name());
    list.set_container_name(Context().container_name());
    vm_tools::apps::App* app = list.add_apps();
    app->set_desktop_file_id(desktop_file_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app->mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(desktop_file_id);
    app->set_no_display(false);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_)
        ->UpdateApplicationList(list);
    return guest_os::GuestOsRegistryService::GenerateAppId(
        desktop_file_id, list.vm_name(), list.container_name());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedCiceroneFake cicerone_;
  TestingProfile profile_;
  std::unique_ptr<BorealisContext> ctx_;
};

TEST_F(BorealisAppLauncherTest, LauncherAppAlwaysWorks) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppLauncher::LaunchResult::kSuccess));
  BorealisAppLauncher::Launch(Context(), kBorealisAppId,
                              callback_check.BindOnce());
}

TEST_F(BorealisAppLauncherTest, UnknownAppCausesError) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppLauncher::LaunchResult::kUnknownApp));
  BorealisAppLauncher::Launch(Context(), "non_existent_app",
                              callback_check.BindOnce());
}

TEST_F(BorealisAppLauncherTest, NoResponseCausesError) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppLauncher::LaunchResult::kNoResponse));
  std::string baz_id = SetDummyApp("foo.desktop");
  Cicerone()->SetOnLaunchContainerApplicationCallback(
      base::BindLambdaForTesting(
          [&](const vm_tools::cicerone::LaunchContainerApplicationRequest&
                  request,
              chromeos::DBusMethodCallback<
                  vm_tools::cicerone::LaunchContainerApplicationResponse>
                  callback) {
            EXPECT_EQ(request.desktop_file_id(), "foo.desktop");
            std::move(callback).Run({});
          }));
  BorealisAppLauncher::Launch(Context(), baz_id, callback_check.BindOnce());
}

TEST_F(BorealisAppLauncherTest, ErrorResponseIsPropagated) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check, Call(BorealisAppLauncher::LaunchResult::kError));
  std::string baz_id = SetDummyApp("bar.desktop");
  Cicerone()->SetOnLaunchContainerApplicationCallback(
      base::BindLambdaForTesting(
          [&](const vm_tools::cicerone::LaunchContainerApplicationRequest&
                  request,
              chromeos::DBusMethodCallback<
                  vm_tools::cicerone::LaunchContainerApplicationResponse>
                  callback) {
            EXPECT_EQ(request.desktop_file_id(), "bar.desktop");
            vm_tools::cicerone::LaunchContainerApplicationResponse response;
            response.set_success(false);
            std::move(callback).Run(response);
          }));
  BorealisAppLauncher::Launch(Context(), baz_id, callback_check.BindOnce());
}

TEST_F(BorealisAppLauncherTest, SuccessfulLaunchHasSuccessResponse) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppLauncher::LaunchResult::kSuccess));
  std::string baz_id = SetDummyApp("baz.desktop");
  Cicerone()->SetOnLaunchContainerApplicationCallback(
      base::BindLambdaForTesting(
          [&](const vm_tools::cicerone::LaunchContainerApplicationRequest&
                  request,
              chromeos::DBusMethodCallback<
                  vm_tools::cicerone::LaunchContainerApplicationResponse>
                  callback) {
            EXPECT_EQ(request.desktop_file_id(), "baz.desktop");
            vm_tools::cicerone::LaunchContainerApplicationResponse response;
            response.set_success(true);
            std::move(callback).Run(response);
          }));
  BorealisAppLauncher::Launch(Context(), baz_id, callback_check.BindOnce());
}

TEST_F(BorealisAppLauncherTest, ApplicationIsRunWithGivenArgs) {
  CallbackFactory callback_check;
  EXPECT_CALL(callback_check,
              Call(BorealisAppLauncher::LaunchResult::kSuccess));
  std::string baz_id = SetDummyApp("baz.desktop");
  Cicerone()->SetOnLaunchContainerApplicationCallback(
      base::BindLambdaForTesting(
          [&](const vm_tools::cicerone::LaunchContainerApplicationRequest&
                  request,
              chromeos::DBusMethodCallback<
                  vm_tools::cicerone::LaunchContainerApplicationResponse>
                  callback) {
            EXPECT_EQ(request.desktop_file_id(), "baz.desktop");
            EXPECT_THAT(
                request.files(),
                testing::Pointwise(testing::Eq(),
                                   {"these", "are", "some", "arguments"}));
            vm_tools::cicerone::LaunchContainerApplicationResponse response;
            response.set_success(true);
            std::move(callback).Run(response);
          }));
  BorealisAppLauncher::Launch(Context(), baz_id,
                              {"these", "are", "some", "arguments"},
                              callback_check.BindOnce());
}

}  // namespace
}  // namespace borealis
