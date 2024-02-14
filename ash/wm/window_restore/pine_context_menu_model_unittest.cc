// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_context_menu_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr char kTestUserEmail[] = "testuser@pine";

constexpr size_t kMenuItemCount = 3u;

}  // namespace

class PineContextMenuModelTest : public AshTestBase {
 public:
  PineContextMenuModelTest() {
    switches::SetIgnoreForestSecretKeyForTest(true);
  }
  PineContextMenuModelTest(const PineContextMenuModelTest&) = delete;
  PineContextMenuModelTest& operator=(const PineContextMenuModelTest) = delete;
  ~PineContextMenuModelTest() override {
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefService as the restore preference is normally
    // registered in chrome/browser/ash/ and is not registered in ash unit
    // tests.
    auto test_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(test_prefs.get()->registry(), /*country=*/"",
                             /*for_test=*/true);
    // Note: normally, this pref is registered with the
    // `user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF` flag.
    test_prefs.get()->registry()->RegisterIntegerPref(
        prefs::kRestoreAppsAndPagesPrefName,
        static_cast<int>(full_restore::RestoreOption::kAskEveryTime));

    session_controller->AddUserSession(kTestUserEmail,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(
        AccountId::FromUserEmail(kTestUserEmail), std::move(test_prefs));

    // Switch to the test user and simulate login.
    session_controller->SwitchActiveUser(
        AccountId::FromUserEmail(kTestUserEmail));
    session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kForestFeature};
};

// Tests that the layout and options of the inline context menu are correct.
TEST_F(PineContextMenuModelTest, LayoutAndCommands) {
  PineContextMenuModel menu;
  ASSERT_EQ(kMenuItemCount, menu.GetItemCount());
  EXPECT_EQ(full_restore::RestoreOption::kAskEveryTime,
            static_cast<full_restore::RestoreOption>(menu.GetCommandIdAt(0)));
  EXPECT_EQ(full_restore::RestoreOption::kAlways,
            static_cast<full_restore::RestoreOption>(menu.GetCommandIdAt(1)));
  EXPECT_EQ(full_restore::RestoreOption::kDoNotRestore,
            static_cast<full_restore::RestoreOption>(menu.GetCommandIdAt(2)));
  for (size_t i = 0; i < kMenuItemCount; ++i) {
    EXPECT_TRUE(menu.IsEnabledAt(i));
    EXPECT_TRUE(menu.IsVisibleAt(i));
  }
}

// Tests that pressing the settings button in Pine properly displays the inline
// context menu.
TEST_F(PineContextMenuModelTest, ShowContextMenuOnSettingsButtonClicked) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();

  // Get the active Pine widget.
  OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(grid);
  auto* pine_widget = grid->pine_widget_for_testing();
  ASSERT_TRUE(pine_widget);

  // The context menu should not be open.
  PineContentsView* contents_view =
      views::AsViewClass<PineContentsView>(pine_widget->GetContentsView());
  ASSERT_TRUE(contents_view);
  EXPECT_FALSE(contents_view->context_menu_model_.get());

  // Click on the settings button, the context menu should appear.
  LeftClickOn(contents_view->settings_button_.get());
  EXPECT_TRUE(contents_view->context_menu_model_.get());
}

// Tests that each menu option properly sets the restore preference when
// activated.
TEST_F(PineContextMenuModelTest, RestorePreferences) {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  ASSERT_TRUE(pref_service);

  // Check the default restore behavior.
  EXPECT_EQ(full_restore::RestoreOption::kAskEveryTime,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));

  // Activate the other menu options (commands) and ensure the preference is set
  // properly.
  PineContextMenuModel menu;
  menu.ActivatedAt(1);
  EXPECT_EQ(full_restore::RestoreOption::kAlways,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));
  menu.ActivatedAt(2);
  EXPECT_EQ(full_restore::RestoreOption::kDoNotRestore,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));
  menu.ActivatedAt(0);
  EXPECT_EQ(full_restore::RestoreOption::kAskEveryTime,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));
}

}  // namespace ash
