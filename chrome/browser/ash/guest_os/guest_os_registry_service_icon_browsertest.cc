// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"

#include <stddef.h>

#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_test.h"

using vm_tools::apps::ApplicationList;

namespace guest_os {

class GuestOsRegistryServiceIconTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    fake_cicerone_client_ = ash::FakeCiceroneClient::Get();
  }

  void TearDownInProcessBrowserTestFixture() override {
    service_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  guest_os::GuestOsRegistryService* service() {
    if (!service_) {
      service_ = std::make_unique<GuestOsRegistryService>(browser()->profile());
    }
    return service_.get();
  }

  void LoadIconAndValidateNoWait(const std::string& app_id,
                                 bool expect_loaded,
                                 base::OnceClosure done_closure) {
    service()->LoadIcon(
        app_id, apps::IconKey(), apps::IconType::kCompressed,
        /*size_hint_in_dip=*/1, /*allow_placeholder_icon=*/false,
        /*fallback_resource_id=*/0,
        base::BindOnce(
            [](bool expect_loaded, base::OnceClosure done_closure,
               apps::IconValuePtr icon) {
              ASSERT_NE(nullptr, icon.get());
              if (expect_loaded) {
                EXPECT_FALSE(icon->is_placeholder_icon);
                EXPECT_EQ(apps::IconType::kCompressed, icon->icon_type);
                EXPECT_GT(icon->compressed.size(), 0u);
              } else {
                EXPECT_EQ(apps::IconType::kUnknown, icon->icon_type);
              }
              std::move(done_closure).Run();
            },
            expect_loaded, std::move(done_closure)));
  }

  void LoadIconAndValidate(const std::string& app_id, bool expect_loaded) {
    base::RunLoop run_loop;
    LoadIconAndValidateNoWait(app_id, expect_loaded, run_loop.QuitClosure());
    run_loop.Run();
    if (expect_loaded) {
      ExpectIconFiles(app_id);
    }
  }

  void ExpectIconFiles(const std::string& app_id) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath icon_dir =
        browser()->profile()->GetPath().Append("crostini.icons").Append(app_id);

    EXPECT_TRUE(base::PathExists(icon_dir.Append("icon.svg")));

    std::string icon_svg_data;

    EXPECT_TRUE(
        base::ReadFileToString(icon_dir.Append("icon.svg"), &icon_svg_data));
    EXPECT_EQ(kSvgData, icon_svg_data);

    // There should also be a transcoded .png file.
    EXPECT_TRUE(base::PathExists(icon_dir.Append("icon_100p.png")));
  }

  std::string AddApp() {
    ApplicationList crostini_list;
    crostini_list.set_vm_type(VmType::TERMINA);
    crostini_list.set_vm_name("termina");
    crostini_list.set_container_name("penguin");
    *crostini_list.add_apps() =
        crostini::CrostiniTestHelper::BasicApp(kSvgAppName);
    std::string app_id = crostini::CrostiniTestHelper::GenerateAppId(
        kSvgAppName, "termina", "penguin");
    service()->UpdateApplicationList(crostini_list);

    vm_tools::cicerone::ContainerAppIconResponse response;
    auto* icon = response.add_icons();
    icon->set_desktop_file_id(app_id);
    icon->set_format(vm_tools::cicerone::DesktopIcon::SVG);
    icon->set_icon(kSvgData);
    fake_cicerone_client_->set_container_app_icon_response(response);

    return app_id;
  }

  void RemoveApps() {
    ApplicationList crostini_list;
    crostini_list.set_vm_type(VmType::TERMINA);
    crostini_list.set_vm_name("termina");
    crostini_list.set_container_name("penguin");
    service()->UpdateApplicationList(crostini_list);
  }

 protected:
  raw_ptr<ash::FakeCiceroneClient, DanglingUntriaged> fake_cicerone_client_;
  static constexpr char kSvgData[] =
      "<svg width='20px' height='20px' viewBox='0 0 24 24' "
      "fill='rgb(95,99,104)' "
      "xmlns='http://www.w3.org/2000/svg'><path d='M0 0h24v24H0V0z' "
      "fill='none'/><path d='M19 19H5V5h7V3H5c-1.11 0-2 .9-2 2v14c0 1.1.89 2 2 "
      "2h14c1.1 0 2-.9 2-2v-7h-2v7zM14 3v2h3.59l-9.83 9.83 1.41 1.41L19 "
      "6.41V10h2V3h-7z'/></svg>";

  static constexpr char kSvgAppName[] = "app_with_svg_icon";

 private:
  std::unique_ptr<GuestOsRegistryService> service_;
};

IN_PROC_BROWSER_TEST_F(GuestOsRegistryServiceIconTest, LoadIconOnce) {
  LoadIconAndValidate(AddApp(), true);
}

IN_PROC_BROWSER_TEST_F(GuestOsRegistryServiceIconTest, LoadIconTwice) {
  std::string app_id = AddApp();
  base::RunLoop run_loop1, run_loop2;
  LoadIconAndValidateNoWait(app_id, true, run_loop1.QuitClosure());
  LoadIconAndValidateNoWait(app_id, true, run_loop2.QuitClosure());
  run_loop1.Run();
  run_loop2.Run();
  ExpectIconFiles(app_id);
}

IN_PROC_BROWSER_TEST_F(GuestOsRegistryServiceIconTest, AddRemoveAddAppIcon) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string app_id = crostini::CrostiniTestHelper::GenerateAppId(
      kSvgAppName, "termina", "penguin");
  base::FilePath icon_dir =
      browser()->profile()->GetPath().Append("crostini.icons").Append(app_id);

  // Initially, there's no app icon.
  EXPECT_FALSE(base::PathExists(icon_dir));
  LoadIconAndValidate(app_id, false);

  // Add the app. There should be an icon.
  AddApp();
  LoadIconAndValidate(app_id, true);

  // RemoveApps, and the icon should go away. We need a FilePathWatcher to know
  // when RemoveApps has finished deleting icons since it is done async.
  base::RunLoop remove_apps_loop;
  base::FilePathWatcher watcher;
  ASSERT_TRUE(watcher.Watch(
      icon_dir, base::FilePathWatcher::Type::kNonRecursive,
      base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
        if (!base::PathExists(icon_dir)) {
          remove_apps_loop.Quit();
        }
      })));
  RemoveApps();
  remove_apps_loop.Run();

  LoadIconAndValidate(app_id, false);

  // Add the app. Ensure 2nd fetch of icon from VM succeeds.
  AddApp();
  LoadIconAndValidate(app_id, true);
}

}  // namespace guest_os
