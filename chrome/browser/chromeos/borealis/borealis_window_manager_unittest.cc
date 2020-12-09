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

using ::testing::_;

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

class MockLifetimeObserver
    : public borealis::BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  MOCK_METHOD(void, OnSessionStarted, (), ());

  MOCK_METHOD(void, OnSessionFinished, (), ());

  MOCK_METHOD(void, OnAppStarted, (const std::string& app_id), ());

  MOCK_METHOD(void, OnAppFinished, (const std::string& app_id), ());

  MOCK_METHOD(void,
              OnWindowStarted,
              (const std::string& app_id, aura::Window*),
              ());

  MOCK_METHOD(void,
              OnWindowFinished,
              (const std::string& app_id, aura::Window*),
              ());

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

TEST_F(BorealisWindowManagerTest, ObserversNotifiedOnManagerShutdown) {
  testing::StrictMock<MockAnonObserver> anon_observer;
  testing::StrictMock<MockLifetimeObserver> life_observer;

  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&anon_observer);
  window_manager.AddObserver(&life_observer);

  EXPECT_CALL(anon_observer, OnWindowManagerDeleted(&window_manager))
      .WillOnce(testing::Invoke([&anon_observer](BorealisWindowManager* wm) {
        wm->RemoveObserver(&anon_observer);
      }));
  EXPECT_CALL(life_observer, OnWindowManagerDeleted(&window_manager))
      .WillOnce(testing::Invoke([&life_observer](BorealisWindowManager* wm) {
        wm->RemoveObserver(&life_observer);
      }));
}

TEST_F(BorealisWindowManagerTest, ObserverCalledForAnonymousApp) {
  testing::StrictMock<MockAnonObserver> observer;
  EXPECT_CALL(observer,
              OnAnonymousAppAdded(testing::ContainsRegex("anonymous_app"), _));

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

TEST_F(BorealisWindowManagerTest, LifetimeObserverTracksWindows) {
  testing::StrictMock<MockLifetimeObserver> observer;
  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&observer);

  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared.
  testing::InSequence sequence;

  // A new window will start everything.
  EXPECT_CALL(observer, OnSessionStarted());
  EXPECT_CALL(observer, OnAppStarted(_));
  EXPECT_CALL(observer, OnWindowStarted(_, _));
  std::unique_ptr<aura::Window> first_foo =
      MakeWindow("org.chromium.borealis.foo");
  window_manager.GetShelfAppId(first_foo.get());

  // A window for the same app only starts that window.
  EXPECT_CALL(observer, OnWindowStarted(_, _));
  std::unique_ptr<aura::Window> second_foo =
      MakeWindow("org.chromium.borealis.foo");
  window_manager.GetShelfAppId(second_foo.get());

  // Whereas a new app starts both the app and the window.
  EXPECT_CALL(observer, OnAppStarted(_));
  EXPECT_CALL(observer, OnWindowStarted(_, _));
  std::unique_ptr<aura::Window> only_bar =
      MakeWindow("org.chromium.borealis.bar");
  window_manager.GetShelfAppId(only_bar.get());

  // Deleting an app window while one still exists does not end the app.
  EXPECT_CALL(observer, OnWindowFinished(_, _));
  first_foo.reset();

  // But deleting them all does finish the app.
  EXPECT_CALL(observer, OnWindowFinished(_, _));
  EXPECT_CALL(observer, OnAppFinished(_));
  second_foo.reset();

  // And deleting all the windows finishes the session.
  EXPECT_CALL(observer, OnWindowFinished(_, _));
  EXPECT_CALL(observer, OnAppFinished(_));
  EXPECT_CALL(observer, OnSessionFinished());
  only_bar.reset();

  window_manager.RemoveObserver(&observer);
}

TEST_F(BorealisWindowManagerTest, HandlesMultipleAnonymousWindows) {
  testing::StrictMock<MockAnonObserver> observer;

  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&observer);

  // We add an anonymous window for the same app twice, but we should only see
  // one observer call.
  EXPECT_CALL(observer, OnAnonymousAppAdded(_, _)).Times(1);

  std::unique_ptr<aura::Window> window1 =
      MakeWindow("org.chromium.borealis.anonymous_app");
  window_manager.GetShelfAppId(window1.get());
  std::unique_ptr<aura::Window> window2 =
      MakeWindow("org.chromium.borealis.anonymous_app");
  window_manager.GetShelfAppId(window2.get());

  // We only expect to see the app removed after the last window closes.
  window1.reset();
  EXPECT_CALL(observer, OnAnonymousAppRemoved(_)).Times(1);
  window2.reset();

  window_manager.RemoveObserver(&observer);
}

TEST_F(BorealisWindowManagerTest, AnonymousObserverNotCalledForKnownApp) {
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
