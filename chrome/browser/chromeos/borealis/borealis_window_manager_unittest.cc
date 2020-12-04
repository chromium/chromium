// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_window_manager.h"

#include <memory>

#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace borealis {
namespace {

class MockAnonObserver
    : public borealis::BorealisWindowManager::AnonymousAppObserver {
 public:
  MOCK_METHOD(void,
              OnAnonymousAppAdded,
              (const std::string&, const std::string&),
              ());

  MOCK_METHOD(void, OnAnonymousAppRemoved, (const std::string&), ());

  MOCK_METHOD(void, OnWindowManagerDeleted, (BorealisWindowManager*), ());
};

class BorealisWindowManagerTest : public testing::Test {
 protected:
  Profile* profile() { return &profile_; }
  // Creates a widget for use in testing.
  std::unique_ptr<aura::Window> MakeWindow(std::string name) {
    auto win = std::make_unique<aura::Window>(nullptr);
    win->Init(ui::LAYER_NOT_DRAWN);
    exo::SetShellApplicationId(win.get(), name);
    return win;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BorealisWindowManagerTest, NonBorealisWindowHasNoId) {
  BorealisWindowManager window_manager(profile());
  std::unique_ptr<aura::Window> window = MakeWindow("not.a.borealis.window");
  EXPECT_EQ(window_manager.GetShelfAppId(window.get()), "");
}

TEST_F(BorealisWindowManagerTest, BorealisWindowHasAnId) {
  BorealisWindowManager window_manager(profile());
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.borealis.foobarbaz");
  EXPECT_NE(window_manager.GetShelfAppId(window.get()), "");
}

TEST_F(BorealisWindowManagerTest, ObserverNotifiedOnManagerShutdown) {
  testing::StrictMock<MockAnonObserver> observer;

  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&observer);

  EXPECT_CALL(observer, OnWindowManagerDeleted(&window_manager))
      .WillOnce(testing::Invoke([&observer](BorealisWindowManager* wm) {
        wm->RemoveObserver(&observer);
      }));
}

TEST_F(BorealisWindowManagerTest, ObserverCalledForAnonymousApp) {
  testing::StrictMock<MockAnonObserver> observer;
  EXPECT_CALL(
      observer,
      OnAnonymousAppAdded(testing::ContainsRegex("anonymous_app"), testing::_));

  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&observer);
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.borealis.anonymous_app");
  window_manager.GetShelfAppId(window.get());

  EXPECT_CALL(observer,
              OnAnonymousAppRemoved(testing::ContainsRegex("anonymous_app")));
  window.reset();

  window_manager.RemoveObserver(&observer);
}

TEST_F(BorealisWindowManagerTest, HandlesMultipleWindows) {
  testing::StrictMock<MockAnonObserver> observer;

  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&observer);

  // We add an anonymous window for the same app twice, but we should only see
  // one observer call.
  EXPECT_CALL(observer, OnAnonymousAppAdded(testing::_, testing::_)).Times(1);

  std::unique_ptr<aura::Window> window1 =
      MakeWindow("org.chromium.borealis.anonymous_app");
  window_manager.GetShelfAppId(window1.get());
  std::unique_ptr<aura::Window> window2 =
      MakeWindow("org.chromium.borealis.anonymous_app");
  window_manager.GetShelfAppId(window2.get());

  // We only expect to see the app removed after the last window closes.
  window1.reset();
  EXPECT_CALL(observer, OnAnonymousAppRemoved(testing::_)).Times(1);
  window2.reset();

  window_manager.RemoveObserver(&observer);
}

TEST_F(BorealisWindowManagerTest, ObserverNotCalledForKnownApp) {
  // Generate a fake app.
  vm_tools::apps::ApplicationList list;
  list.set_vm_name("vm");
  list.set_container_name("container");
  list.set_vm_type(vm_tools::apps::ApplicationList_VmType_BOREALIS);
  vm_tools::apps::App* app = list.add_apps();
  app->set_desktop_file_id("foo.desktop");
  app->mutable_name()->add_values()->set_value("foo");
  app->set_no_display(false);
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile())
      ->UpdateApplicationList(list);

  testing::StrictMock<MockAnonObserver> observer;

  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&observer);
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.borealis.wmclass.foo");
  window_manager.GetShelfAppId(window.get());

  window_manager.RemoveObserver(&observer);
}

}  // namespace
}  // namespace borealis
