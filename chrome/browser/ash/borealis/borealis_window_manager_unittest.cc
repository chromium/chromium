// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_window_manager.h"

#include <memory>

#include "ash/wm/window_state.h"
#include "base/base64.h"
#include "chrome/browser/ash/borealis/borealis_window_manager_mock.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/borealis/borealis_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

using ::testing::_;

namespace borealis {
namespace {

class BorealisWindowManagerTest : public ChromeAshTestBase {
 protected:
  Profile* profile() { return &profile_; }

  void SetUp() override { ChromeAshTestBase::SetUp(); }

  void TearDown() override { ChromeAshTestBase::TearDown(); }

  std::unique_ptr<views::Widget> SetupMinimizeTest(
      std::string active_window_id) {
    // Windows should only ever be minimised when an active borealis window is
    // fullscreen.
    std::unique_ptr<views::Widget> active_widget =
        CreateFakeWidget(active_window_id, true);
    aura::Window* window = active_widget->GetNativeWindow();
    EXPECT_TRUE(ash::WindowState::Get(window)->IsFullscreen());
    return active_widget;
  }

  std::string GetBorealisClientId() {
    return std::string(ash::borealis::kBorealisWindowPrefix) +
           kBorealisClientSuffix;
  }

 private:
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
      MakeWindow("org.chromium.guest_os.borealis.foobarbaz");
  EXPECT_NE(window_manager.GetShelfAppId(window.get()), "");
}

TEST_F(BorealisWindowManagerTest, BorealisWindowHasCorrectId) {
  BorealisWindowManager window_manager(profile());
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.xprop.456789");
  CreateFakeApp(profile(), "some_app", "steam://rungameid/456789");
  EXPECT_EQ(window_manager.GetShelfAppId(window.get()), FakeAppId("some_app"));
}

TEST_F(BorealisWindowManagerTest, MismatchedWindowHasDifferentId) {
  BorealisWindowManager window_manager(profile());
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.xprop.2468");
  CreateFakeApp(profile(), "some_app", "steam://rungameid/456789");
  EXPECT_NE(window_manager.GetShelfAppId(window.get()), FakeAppId("some_app"));
}

TEST_F(BorealisWindowManagerTest, IdDetectionDoesNotImplyTracking) {
  BorealisWindowManager window_manager(profile());

  testing::StrictMock<MockAnonObserver> anon_observer;
  testing::StrictMock<MockLifetimeObserver> life_observer;
  window_manager.AddObserver(&anon_observer);
  window_manager.AddObserver(&life_observer);

  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.foobarbaz");
  window_manager.GetShelfAppId(window.get());

  window_manager.RemoveObserver(&anon_observer);
  window_manager.RemoveObserver(&life_observer);
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
  std::unique_ptr<ScopedTestWindow> window = MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.anonymous_app", &window_manager);

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
  std::unique_ptr<ScopedTestWindow> first_foo =
      MakeAndTrackWindow("org.chromium.guest_os.borealis.foo", &window_manager);

  // A window for the same app only starts that window.
  EXPECT_CALL(observer, OnWindowStarted(_, _));
  std::unique_ptr<ScopedTestWindow> second_foo =
      MakeAndTrackWindow("org.chromium.guest_os.borealis.foo", &window_manager);

  // Whereas a new app starts both the app and the window.
  EXPECT_CALL(observer, OnAppStarted(_));
  EXPECT_CALL(observer, OnWindowStarted(_, _));
  std::unique_ptr<ScopedTestWindow> only_bar =
      MakeAndTrackWindow("org.chromium.guest_os.borealis.bar", &window_manager);

  // Deleting an app window while one still exists does not end the app.
  EXPECT_CALL(observer, OnWindowFinished(_, _));
  first_foo.reset();

  // But deleting them all does finish the app.
  EXPECT_CALL(observer, OnWindowFinished(_, _));
  EXPECT_CALL(observer, OnAppFinished(_, _));
  second_foo.reset();

  // And deleting all the windows finishes the session.
  EXPECT_CALL(observer, OnWindowFinished(_, _));
  EXPECT_CALL(observer, OnAppFinished(_, _));
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

  std::unique_ptr<ScopedTestWindow> window1 = MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.anonymous_app", &window_manager);
  std::unique_ptr<ScopedTestWindow> window2 = MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.anonymous_app", &window_manager);

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
  list.set_vm_type(vm_tools::apps::BOREALIS);
  vm_tools::apps::App* app = list.add_apps();
  app->set_desktop_file_id("foo.desktop");
  app->mutable_name()->add_values()->set_value("foo");
  app->set_no_display(false);
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile())
      ->UpdateApplicationList(list);

  testing::StrictMock<MockAnonObserver> observer;

  BorealisWindowManager window_manager(profile());
  window_manager.AddObserver(&observer);
  std::unique_ptr<ScopedTestWindow> window = MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.wmclass.foo", &window_manager);

  window_manager.RemoveObserver(&observer);
}

TEST_F(BorealisWindowManagerTest, SteamClientIsNonGameBorealisWindow) {
  std::unique_ptr<aura::Window> window = MakeWindow(GetBorealisClientId());

  EXPECT_TRUE(ash::borealis::IsBorealisWindow(window.get()));
  EXPECT_FALSE(
      BorealisWindowManager::IsSteamGameWindow(profile(), window.get()));
}

TEST_F(BorealisWindowManagerTest, NewSteamClientIsNonGameBorealisWindow) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.xprop.769");

  EXPECT_TRUE(ash::borealis::IsBorealisWindow(window.get()));
  EXPECT_FALSE(
      BorealisWindowManager::IsSteamGameWindow(profile(), window.get()));
}

TEST_F(BorealisWindowManagerTest, ArbitraryBorealisWindowsAreNotGames) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.foo");

  EXPECT_TRUE(ash::borealis::IsBorealisWindow(window.get()));
  EXPECT_FALSE(
      BorealisWindowManager::IsSteamGameWindow(profile(), window.get()));
}
TEST_F(BorealisWindowManagerTest, CanIdentifySteamGames) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.borealis.xprop.123");

  EXPECT_TRUE(ash::borealis::IsBorealisWindow(window.get()));
  EXPECT_TRUE(
      BorealisWindowManager::IsSteamGameWindow(profile(), window.get()));
}

TEST_F(BorealisWindowManagerTest, TerminaWindowsAreNotBorealisWindowsOrGames) {
  std::unique_ptr<aura::Window> window =
      MakeWindow("org.chromium.guest_os.termina.xprop.123");

  EXPECT_FALSE(ash::borealis::IsBorealisWindow(window.get()));
  EXPECT_FALSE(
      BorealisWindowManager::IsSteamGameWindow(profile(), window.get()));
}

}  // namespace
}  // namespace borealis
