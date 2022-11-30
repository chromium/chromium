// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/crostini/crostini_browser_test_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "content/public/test/browser_test.h"

namespace crostini {

namespace {}

class CrostiniBrowserTest : public CrostiniBrowserTestBase {
 public:
  CrostiniBrowserTest() : CrostiniBrowserTestBase(/*register_termina=*/true) {}

  void RegisterApp() {
    vm_tools::apps::App app;
    app.set_desktop_file_id("app_id");

    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value("app_name");

    vm_tools::apps::ApplicationList app_list;
    app_list.set_vm_name(kCrostiniDefaultVmName);
    app_list.set_container_name(kCrostiniDefaultContainerName);
    *app_list.add_apps() = app;

    guest_os::GuestOsRegistryServiceFactory::GetForProfile(browser()->profile())
        ->UpdateApplicationList(app_list);
  }

  void LaunchApp() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    LaunchCrostiniApp(browser()->profile(), kAppId, display::kInvalidDisplayId,
                      {},
                      base::BindOnce(
                          [](base::OnceClosure quit_closure, bool success,
                             const std::string& failure_reason) {
                            EXPECT_TRUE(success) << failure_reason;
                            std::move(quit_closure).Run();
                          },
                          run_loop.QuitClosure()));
    run_loop.Run();
  }

  const std::string kAppId = guest_os::GuestOsRegistryService::GenerateAppId(
      "app_id",
      kCrostiniDefaultVmName,
      kCrostiniDefaultContainerName);
};

IN_PROC_BROWSER_TEST_F(CrostiniBrowserTest, LaunchApplication) {
  RegisterApp();
  LaunchApp();
}

class CrostiniForbiddenBrowserTest : public CrostiniBrowserTest {
 public:
  CrostiniForbiddenBrowserTest() {
    fake_crostini_features_.set_could_be_allowed(true);
    fake_crostini_features_.set_is_allowed_now(false);
  }
};

// Test launching an application when enterprise policy changed during the
// session to allow crostini.
IN_PROC_BROWSER_TEST_F(CrostiniForbiddenBrowserTest, LaunchApplication) {
  fake_crostini_features_.set_is_allowed_now(true);
  RegisterApp();
  LaunchApp();
}

}  // namespace crostini
