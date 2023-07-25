// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_launcher.h"

#include <memory>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

using CallbackFactory =
    StrictCallbackFactory<void(BorealisAppLauncher::LaunchResult)>;

class BorealisAppLauncherTest : public testing::Test,
                                protected guest_os::FakeVmServicesHelper {
 public:
  BorealisAppLauncherTest()
      : ctx_(BorealisContext::CreateBorealisContextForTesting(&profile_)) {
    ctx_->set_vm_name("borealis");
    ctx_->set_container_name("penguin");
  }

 protected:
  const BorealisContext& Context() { return *ctx_; }

  Profile* profile() { return &profile_; }

  // Sets up the registry with a single app. Returns its app id.
  std::string SetDummyApp(const std::string& desktop_file_id) {
    CreateFakeApp(profile(), desktop_file_id, /*exec=*/{});
    return FakeAppId(desktop_file_id);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BorealisContext> ctx_;
};

TEST_F(BorealisAppLauncherTest, LauncherAppLaunchesMainApp) {
  CallbackFactory callback_check;
  CreateFakeMainApp(profile());

  EXPECT_CALL(callback_check,
              Call(BorealisAppLauncher::LaunchResult::kSuccess));
  FakeCiceroneClient()->SetOnLaunchContainerApplicationCallback(
      base::BindLambdaForTesting(
          [&](const vm_tools::cicerone::LaunchContainerApplicationRequest&
                  request,
              chromeos::DBusMethodCallback<
                  vm_tools::cicerone::LaunchContainerApplicationResponse>
                  callback) {
            vm_tools::cicerone::LaunchContainerApplicationResponse response;
            response.set_success(true);
            std::move(callback).Run(response);
          }));
  BorealisAppLauncher::Launch(Context(), kInstallerAppId,
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
  FakeCiceroneClient()->SetOnLaunchContainerApplicationCallback(
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
  FakeCiceroneClient()->SetOnLaunchContainerApplicationCallback(
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
  FakeCiceroneClient()->SetOnLaunchContainerApplicationCallback(
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
  FakeCiceroneClient()->SetOnLaunchContainerApplicationCallback(
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
