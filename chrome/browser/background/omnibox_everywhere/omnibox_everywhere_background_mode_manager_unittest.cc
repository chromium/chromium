// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_icon.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(IS_WIN)
#include <optional>

#include "base/test/run_until.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/startup/startup_features.h"
#include "chrome/browser/startup/startup_launch_manager.h"
#include "chrome/browser/status_icons/status_tray_theme_watcher_win_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#endif

namespace omnibox_everywhere {

namespace {

// The DIP size of the search loupe in the status icon.
constexpr int kLoupeDipSize =
#if BUILDFLAG(IS_WIN)
    32;
#else
    16;
#endif

class MockStatusIcon : public StatusIcon {
 public:
  explicit MockStatusIcon(const gfx::ImageSkia& image) : image_(image) {}
  ~MockStatusIcon() override = default;

  void SetImage(const gfx::ImageSkia& image) override {
    image_ = image;
    ++set_image_count_;
#if BUILDFLAG(IS_MAC)
    // Like `StatusIconMac`, a new image is not a template image.
    is_template_ = false;
#endif
  }
#if BUILDFLAG(IS_MAC)
  void SetImageTemplate(bool is_template) override {
    is_template_ = is_template;
  }
#endif
  void SetToolTip(const std::u16string& tool_tip) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}

  const gfx::ImageSkia& image() const { return image_; }
  int set_image_count() const { return set_image_count_; }
#if BUILDFLAG(IS_MAC)
  bool is_template() const { return is_template_; }
#endif

 private:
  gfx::ImageSkia image_;
  int set_image_count_ = 0;
#if BUILDFLAG(IS_MAC)
  bool is_template_ = false;
#endif
};

class MockStatusTray : public StatusTray {
 public:
  MockStatusTray() = default;
  ~MockStatusTray() override = default;

  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    return std::make_unique<MockStatusIcon>(image);
  }
};

#if BUILDFLAG(IS_WIN)
class TestStartupLaunchManager : public StartupLaunchManager {
 public:
  explicit TestStartupLaunchManager(BrowserProcess* browser_process)
      : StartupLaunchManager(browser_process) {}

  MOCK_METHOD1(
      UpdateLaunchOnStartup,
      void(std::optional<auto_launch_util::StartupLaunchMode> startup_mode));
};
#endif

}  // namespace

class OmniboxEverywhereBackgroundModeManagerTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereBackgroundModeManagerTest() = default;
  ~OmniboxEverywhereBackgroundModeManagerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
    TestingBrowserProcess::GetGlobal()->SetStatusTray(
        std::make_unique<MockStatusTray>());
    if (PrefService* local_state =
            TestingBrowserProcess::GetGlobal()->local_state()) {
      local_state->SetBoolean(prefs::kOmniboxEverywhereEnabled, true);
      local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
    }
#if BUILDFLAG(IS_WIN)
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    SetTaskbarDarkModeInRegistry(false);
#endif
  }

  void TearDown() override {
    profile_.reset();
    TestingBrowserProcess::GetGlobal()->SetStatusTray(nullptr);
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
    AfterStartupTaskUtils::UnsafeResetForTesting();
    ChromeViewsTestBase::TearDown();
  }

#if BUILDFLAG(IS_WIN)
  TestStartupLaunchManager* startup_launch_manager() {
    return static_cast<TestStartupLaunchManager*>(
        StartupLaunchManager::From(g_browser_process));
  }

  void ExpectStartupRegistration(bool launch_enabled) {
    std::optional<auto_launch_util::StartupLaunchMode> launch_mode;
    if (launch_enabled) {
      launch_mode = auto_launch_util::StartupLaunchMode::kBackground;
    }
    EXPECT_CALL(*startup_launch_manager(), UpdateLaunchOnStartup(launch_mode))
        .Times(testing::AtLeast(1));
  }

  void VerifyAndClearStartupRegistrationExpectations() {
    testing::Mock::VerifyAndClearExpectations(startup_launch_manager());
  }
#endif

  TestingProfile* profile() {
    if (!profile_) {
      profile_ = std::make_unique<TestingProfile>();
    }
    return profile_.get();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{omnibox::kOmniboxEverywhere};
  std::unique_ptr<TestingProfile> profile_;
#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager registry_override_;
#endif
};

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, InitializationDoesNotCrash) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, EnabledPrefToggle) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  manager.SetProfile(profile());

  // Initially enabled pref is true, status icon should exist.
  EXPECT_NE(manager.status_icon_for_testing(), nullptr);

  // Toggle enabled pref off and on.
  if (local_state) {
    local_state->SetBoolean(prefs::kOmniboxEverywhereEnabled, false);
    EXPECT_EQ(manager.status_icon_for_testing(), nullptr);
    local_state->SetBoolean(prefs::kOmniboxEverywhereEnabled, true);
    EXPECT_NE(manager.status_icon_for_testing(), nullptr);
  }
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       BackgroundModePrefDoesNotAffectStatusIcon) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  manager.SetProfile(profile());

  EXPECT_NE(manager.status_icon_for_testing(), nullptr);

  // Toggle background mode pref off and on. Status icon should remain
  // unaffected.
  if (local_state) {
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
    EXPECT_NE(manager.status_icon_for_testing(), nullptr);
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
    EXPECT_NE(manager.status_icon_for_testing(), nullptr);
  }
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       RequiresProfileToShowStatusIcon) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Status icon is not shown without a profile even if the enabled pref is
  // true.
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);

  // Status icon is shown once a profile is set.
  manager.SetProfile(profile());
  EXPECT_NE(manager.status_icon_for_testing(), nullptr);

  // Clearing the profile removes the status icon.
  manager.SetProfile(nullptr);
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);

  // Setting the profile again restores the status icon.
  manager.SetProfile(profile());
  EXPECT_NE(manager.status_icon_for_testing(), nullptr);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       StatusIconFollowsDefaultSearchEngine) {
  const gfx::Image expected_loupe(gfx::CreateVectorIcon(
      vector_icons::kSearchIcon, kLoupeDipSize, SK_ColorBLACK));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(gfx::Image(GetOmniboxEverywhereIcon(
                                    nullptr, /*for_dark_background=*/false)),
                                expected_loupe));
  // On a dark background, the loupe is white.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(
          GetOmniboxEverywhereIcon(nullptr, /*for_dark_background=*/true)),
      gfx::Image(gfx::CreateVectorIcon(vector_icons::kSearchIcon, kLoupeDipSize,
                                       SK_ColorWHITE))));

  TemplateURLServiceFactoryTestUtil template_url_service_util(profile());
  template_url_service_util.VerifyLoad();
  TemplateURLService* template_url_service = template_url_service_util.model();
  auto set_default_search_provider = [&](bool is_google) {
    TemplateURLData data;
    data.SetShortName(is_google ? u"Google" : u"Other");
    data.SetKeyword(is_google ? u"google.com" : u"other.com");
    data.SetURL(is_google ? "https://www.google.com/search?q={searchTerms}"
                          : "https://www.other.com/search?q={searchTerms}");
    template_url_service->SetUserSelectedDefaultSearchProvider(
        template_url_service->Add(std::make_unique<TemplateURL>(data)));
    return gfx::Image(GetOmniboxEverywhereIcon(template_url_service,
                                               /*for_dark_background=*/false));
  };
  const gfx::Image google_icon = set_default_search_provider(true);
  EXPECT_EQ(gfx::test::AreImagesEqual(google_icon, expected_loupe),
            !BUILDFLAG(GOOGLE_CHROME_BRANDING));
  // The Google "G" keeps its colors on a dark background, unlike the loupe.
  EXPECT_EQ(gfx::test::AreImagesEqual(
                gfx::Image(GetOmniboxEverywhereIcon(
                    template_url_service, /*for_dark_background=*/true)),
                google_icon),
            BUILDFLAG(GOOGLE_CHROME_BRANDING));

  OmniboxEverywhereBackgroundModeManager manager(base::DoNothing());
  manager.SetProfile(profile());
  auto* status_icon =
      static_cast<MockStatusIcon*>(manager.status_icon_for_testing());
  ASSERT_NE(status_icon, nullptr);
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(gfx::Image(status_icon->image()), google_icon));
#if BUILDFLAG(IS_MAC)
  // Only the monochrome loupe is a template image.
  EXPECT_EQ(status_icon->is_template(), !BUILDFLAG(GOOGLE_CHROME_BRANDING));
#endif

  // Switching to a non-Google search engine updates the status icon to the
  // generic search loupe.
  const gfx::Image other_icon = set_default_search_provider(false);
  EXPECT_TRUE(gfx::test::AreImagesEqual(other_icon, expected_loupe));
  const int count_after_other = status_icon->set_image_count();
  EXPECT_GT(count_after_other, 0);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(status_icon->image()),
                                        expected_loupe));
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(status_icon->is_template());
#endif

  // Switching back to Google restores the Google icon.
  set_default_search_provider(true);
  const int count_after_google = status_icon->set_image_count();
  EXPECT_GT(count_after_google, count_after_other);
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(gfx::Image(status_icon->image()), google_icon));

  // Switching to another profile while the status icon is already visible
  // refreshes the icon for the new profile.
  TestingProfile other_profile;
  manager.SetProfile(&other_profile);
  EXPECT_GT(status_icon->set_image_count(), count_after_google);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(status_icon->image()),
                                        expected_loupe));
  manager.SetProfile(profile());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(gfx::Image(status_icon->image()), google_icon));
}

#if BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       StatusIconFollowsTaskbarTheme) {
  // `profile()` has no default search engine, so the status icon is the loupe.
  OmniboxEverywhereBackgroundModeManager manager(base::DoNothing());
  manager.SetProfile(profile());
  auto* status_icon =
      static_cast<MockStatusIcon*>(manager.status_icon_for_testing());
  ASSERT_NE(status_icon, nullptr);
  auto shows_loupe = [&](SkColor color) {
    return gfx::test::AreImagesEqual(
        gfx::Image(status_icon->image()),
        gfx::Image(gfx::CreateVectorIcon(vector_icons::kSearchIcon,
                                         kLoupeDipSize, color)));
  };
  EXPECT_TRUE(shows_loupe(SK_ColorBLACK));

  SetTaskbarDarkModeInRegistry(true);
  EXPECT_TRUE(base::test::RunUntil([&] { return shows_loupe(SK_ColorWHITE); }));

  SetTaskbarDarkModeInRegistry(false);
  EXPECT_TRUE(base::test::RunUntil([&] { return shows_loupe(SK_ColorBLACK); }));
}
#endif

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       StatusIconClickTriggersCallback) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Simulate status icon click callback execution
  static_cast<StatusIconObserver*>(&manager)->OnStatusIconClicked();
  EXPECT_TRUE(callback_called);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, ContextMenuStructure) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  manager.SetProfile(profile());

  StatusIcon* status_icon = manager.status_icon_for_testing();
  ASSERT_NE(status_icon, nullptr);

  StatusIconMenuModel* menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  // Initially no browsers are open in this test, so the exit item and separator
  // are visible.
  ASSERT_EQ(menu->GetItemCount(), 6u);
  EXPECT_EQ(menu->GetTypeAt(4), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetCommandIdAt(5),
            IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT);
  EXPECT_TRUE(
      menu->IsCommandIdVisible(IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT));
#else
  ASSERT_EQ(menu->GetItemCount(), 4u);
#endif

  EXPECT_EQ(menu->GetCommandIdAt(0),
            IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE);
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(
      menu->GetCommandIdAt(2),
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT);
  EXPECT_EQ(menu->GetCommandIdAt(3),
            IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS);

  ui::Accelerator accelerator;
  EXPECT_FALSE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       ContextMenuUpdatesOnCustomHotkeyChange) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  manager.SetProfile(profile());

  StatusIcon* status_icon = manager.status_icon_for_testing();
  ASSERT_NE(status_icon, nullptr);

  StatusIconMenuModel* menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);

  ui::Accelerator accelerator;
  EXPECT_FALSE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));

  // Update custom hotkey pref and verify the status icon context menu updates.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetString(
      prefs::kOmniboxEverywhereHotkey, "Ctrl+Shift+Space");

  menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);
  EXPECT_TRUE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));
  EXPECT_EQ(
      accelerator,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN));

  // Clear hotkey and verify accelerator is removed from context menu.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetString(
      prefs::kOmniboxEverywhereHotkey, "");

  menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);
  EXPECT_FALSE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));

  // Re-set hotkey, then disable hotkey and verify accelerator is removed.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetString(
      prefs::kOmniboxEverywhereHotkey, "Ctrl+Shift+Space");

  menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);
  EXPECT_TRUE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));

  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetBoolean(
      prefs::kHotkeyEnabled, false);

  menu = status_icon->GetContextMenuForTesting();
  ASSERT_NE(menu, nullptr);
  EXPECT_FALSE(menu->GetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &accelerator));
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, ExecuteToggleCommand) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  StatusIconMenuModel::Delegate* delegate =
      static_cast<StatusIconMenuModel::Delegate*>(&manager);
  delegate->ExecuteCommand(IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, 0);
  EXPECT_TRUE(callback_called);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
TEST_F(OmniboxEverywhereBackgroundModeManagerTest, ExecuteExitCommand) {
  base::UserActionTester user_action_tester;
  bool show_callback_called = false;
  bool close_callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(
      base::BindRepeating([](bool* called) { *called = true; },
                          &show_callback_called),
      base::BindRepeating([](bool* called) { *called = true; },
                          &close_callback_called));

  StatusIconMenuModel::Delegate* delegate =
      static_cast<StatusIconMenuModel::Delegate*>(&manager);
  delegate->ExecuteCommand(IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT, 0);

  // The Omnibox Everywhere window is closed along with the browsers, as it
  // holds a keep-alive that would otherwise prevent shutdown.
  EXPECT_TRUE(close_callback_called);
  EXPECT_FALSE(show_callback_called);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Exit"));
}
#endif

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       LaunchOnStartupPrefToggleWithoutStartupLaunchManagerDoesNotCrash) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  ASSERT_NE(local_state, nullptr);

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Toggling launch on startup pref should not crash even when
  // StartupLaunchManager is not initialized in test environment.
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, false);
}

#if BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       LaunchOnStartupRegistrationWithStartupLaunchManager) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {omnibox::kOmniboxEverywhere},
      {features::kLaunchOnStartup, features::kLaunchOnStartupInfoBar});

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  ASSERT_NE(local_state, nullptr);

  auto scoped_override =
      GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
          base::BindRepeating([](BrowserProcess& browser_process) {
            return std::make_unique<TestStartupLaunchManager>(&browser_process);
          }));

  // Reset any profile before replacing GlobalFeatures so keyed services (such
  // as NtpBackgroundService) do not retain dangling raw_refs to
  // GlobalFeatures components like ApplicationLocaleStorage.
  profile_.reset();
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, false);

  // Consume any initial startup registration calls during setup.
  EXPECT_CALL(*startup_launch_manager(), UpdateLaunchOnStartup(testing::_))
      .Times(testing::AnyNumber());
  startup_launch_manager()->CommitLaunchOnStartupState();

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
  VerifyAndClearStartupRegistrationExpectations();

  // 1. Enabling launch on startup registers background launch.
  ExpectStartupRegistration(/*launch_enabled=*/true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, true);
  VerifyAndClearStartupRegistrationExpectations();

  // 2. Disabling launch on startup unregisters background launch.
  ExpectStartupRegistration(/*launch_enabled=*/false);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, false);
  VerifyAndClearStartupRegistrationExpectations();

  // 3. Re-enabling launch on startup registers background launch again.
  ExpectStartupRegistration(/*launch_enabled=*/true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereLaunchOnStartup, true);
  VerifyAndClearStartupRegistrationExpectations();

  // 4. Disabling background mode unregisters startup launch even if
  // launch_on_startup pref is true.
  ExpectStartupRegistration(/*launch_enabled=*/false);
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
  VerifyAndClearStartupRegistrationExpectations();

  // 5. Re-enabling background mode registers background launch again.
  ExpectStartupRegistration(/*launch_enabled=*/true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  VerifyAndClearStartupRegistrationExpectations();

  // 6. Disabling enabled pref unregisters startup launch even if
  // launch_on_startup and background_mode prefs are true.
  ExpectStartupRegistration(/*launch_enabled=*/false);
  local_state->SetBoolean(prefs::kOmniboxEverywhereEnabled, false);
  VerifyAndClearStartupRegistrationExpectations();

  // 7. Re-enabling enabled pref restores background launch registration.
  ExpectStartupRegistration(/*launch_enabled=*/true);
  local_state->SetBoolean(prefs::kOmniboxEverywhereEnabled, true);
  VerifyAndClearStartupRegistrationExpectations();

  profile_.reset();
  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
}
#endif

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       DeferStatusIconUntilBrowserStartupComplete) {
  Profile* test_profile = profile();
  AfterStartupTaskUtils::UnsafeResetForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {omnibox::kOmniboxEverywhere, omnibox::kOmniboxEverywhereDeferStatusIcon},
      {});

  OmniboxEverywhereBackgroundModeManager manager(base::DoNothing());

  manager.SetProfile(test_profile);

  // Keep-alive must be acquired synchronously during `SetProfile()`, while
  // status icon and context menu creation are deferred until browser startup
  // completes.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE));
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);
  EXPECT_EQ(manager.context_menu_for_testing(), nullptr);

  // Running the UI thread until idle while browser startup is still incomplete
  // must not create the status icon.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);
  EXPECT_EQ(manager.context_menu_for_testing(), nullptr);

  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  task_environment()->RunUntilIdle();
  EXPECT_NE(manager.status_icon_for_testing(), nullptr);
  EXPECT_NE(manager.context_menu_for_testing(), nullptr);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       StatusIconCreatedSynchronouslyWhenDeferFeatureDisabled) {
  Profile* test_profile = profile();
  AfterStartupTaskUtils::UnsafeResetForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {omnibox::kOmniboxEverywhere},
      {omnibox::kOmniboxEverywhereDeferStatusIcon});

  OmniboxEverywhereBackgroundModeManager manager(base::DoNothing());
  manager.SetProfile(test_profile);

  EXPECT_NE(manager.status_icon_for_testing(), nullptr);
  EXPECT_NE(manager.context_menu_for_testing(), nullptr);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       DeferredStatusIconCancelledOnReset) {
  Profile* test_profile = profile();
  AfterStartupTaskUtils::UnsafeResetForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {omnibox::kOmniboxEverywhere, omnibox::kOmniboxEverywhereDeferStatusIcon},
      {});

  OmniboxEverywhereBackgroundModeManager manager(base::DoNothing());

  manager.SetProfile(test_profile);
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);

  // Clearing the profile before the deferred task runs must cancel status
  // icon creation and release the keep-alive.
  manager.SetProfile(nullptr);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE));
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       DeferredStatusIconCancelledAndRearmedOnPrefToggle) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  ASSERT_NE(local_state, nullptr);
  Profile* test_profile = profile();
  AfterStartupTaskUtils::UnsafeResetForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {omnibox::kOmniboxEverywhere, omnibox::kOmniboxEverywhereDeferStatusIcon},
      {});

  OmniboxEverywhereBackgroundModeManager manager(base::DoNothing());
  manager.SetProfile(test_profile);
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);

  // Disabling and re-enabling before startup completes must cancel the prior
  // task and schedule a fresh deferred task.
  local_state->SetBoolean(prefs::kOmniboxEverywhereEnabled, false);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE));
  local_state->SetBoolean(prefs::kOmniboxEverywhereEnabled, true);
  EXPECT_EQ(manager.status_icon_for_testing(), nullptr);

  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  task_environment()->RunUntilIdle();
  EXPECT_NE(manager.status_icon_for_testing(), nullptr);
  EXPECT_NE(manager.context_menu_for_testing(), nullptr);
}

}  // namespace omnibox_everywhere
