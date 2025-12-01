// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/chromevox_test_utils.h"
#include "chrome/browser/ash/accessibility/spoken_feedback_browsertest.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/user_manager/user_names.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

using SpokenFeedbackTestVariant = ash::SpokenFeedbackTestVariant;
using SpokenFeedbackTestConfig = ::ash::SpokenFeedbackTestConfig;

namespace ash {

namespace {

void SendKeyPressWithShiftAndControl(ui::test::EventGenerator* generator,
                                     ui::KeyboardCode key) {
  const int flags = ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;
  generator->PressAndReleaseKeyAndModifierKeys(key, flags);
}

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, double relevance) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    SetDisplayScore(relevance);
  }

  TestSearchResult(const TestSearchResult&) = delete;
  TestSearchResult& operator=(const TestSearchResult&) = delete;

  ~TestSearchResult() override = default;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

class TestSearchProvider : public app_list::SearchProvider {
 public:
  TestSearchProvider(const std::string& prefix,
                     ChromeSearchResult::DisplayType display_type,
                     ChromeSearchResult::Category category,
                     ChromeSearchResult::ResultType result_type,
                     app_list::SearchCategory search_category)
      : SearchProvider(search_category),
        prefix_(prefix),
        display_type_(display_type),
        category_(category),
        result_type_(result_type) {}

  TestSearchProvider(const TestSearchProvider&) = delete;
  TestSearchProvider& operator=(const TestSearchProvider&) = delete;

  ~TestSearchProvider() override = default;

  // SearchProvider overrides:
  void Start(const std::u16string& query) override {
    auto create_result =
        [this](int index) -> std::unique_ptr<ChromeSearchResult> {
      const std::string id =
          base::StringPrintf("%s %d", prefix_.c_str(), index);
      double relevance = 1.0f - index / 100.0;
      auto result = std::make_unique<TestSearchResult>(id, relevance);

      result->SetDisplayType(display_type_);
      result->SetCategory(category_);
      result->SetResultType(result_type_);

      return result;
    };

    std::vector<std::unique_ptr<ChromeSearchResult>> results;
    for (size_t i = 0; i < count_ + best_match_count_; ++i) {
      std::unique_ptr<ChromeSearchResult> result = create_result(i);
      result->SetBestMatch(i < best_match_count_);
      if (result_type_ == ChromeSearchResult::ResultType::kImageSearch) {
        result->SetIcon(ChromeSearchResult::IconInfo(
            ui::ImageModel::FromVectorIcon(vector_icons::kGoogleColorIcon),
            /*dimension=*/100));
      }
      results.push_back(std::move(result));
    }

    SwapResults(&results);
  }

  ChromeSearchResult::ResultType ResultType() const override {
    return result_type_;
  }

  void set_count(size_t count) { count_ = count; }
  void set_best_match_count(size_t count) { best_match_count_ = count; }

 private:
  std::string prefix_;
  size_t count_ = 0;
  size_t best_match_count_ = 0;
  ChromeSearchResult::DisplayType display_type_;
  ChromeSearchResult::Category category_;
  ChromeSearchResult::ResultType result_type_;
};

// Adds two test providers to `search_controller` - one for app results, and
// another one for omnibox results. Returns pointers to created providers
// through `apps_provder_ptr` and `web_provider_ptr`.
void InitializeTestSearchProviders(
    app_list::SearchController* search_controller,
    raw_ptr<TestSearchProvider>* apps_provider_ptr,
    raw_ptr<TestSearchProvider>* web_provider_ptr,
    raw_ptr<TestSearchProvider>* image_provider_ptr) {
  std::unique_ptr<TestSearchProvider> apps_provider =
      std::make_unique<TestSearchProvider>(
          "app", ChromeSearchResult::DisplayType::kList,
          ChromeSearchResult::Category::kApps,
          ChromeSearchResult::ResultType::kInstalledApp,
          app_list::SearchCategory::kApps);
  *apps_provider_ptr = apps_provider.get();
  search_controller->AddProvider(std::move(apps_provider));

  std::unique_ptr<TestSearchProvider> web_provider =
      std::make_unique<TestSearchProvider>(
          "item", ChromeSearchResult::DisplayType::kList,
          ChromeSearchResult::Category::kWeb,
          ChromeSearchResult::ResultType::kOmnibox,
          app_list::SearchCategory::kWeb);
  *web_provider_ptr = web_provider.get();
  search_controller->AddProvider(std::move(web_provider));

  std::unique_ptr<TestSearchProvider> image_provider =
      std::make_unique<TestSearchProvider>(
          "image", ChromeSearchResult::DisplayType::kImage,
          ChromeSearchResult::Category::kFiles,
          ChromeSearchResult::ResultType::kImageSearch,
          app_list::SearchCategory::kImages);
  *image_provider_ptr = image_provider.get();
  search_controller->AddProvider(std::move(image_provider));
}

}  // namespace

class SpokenFeedbackAppListBaseTest : public LoggedInSpokenFeedbackTest {
 public:
  explicit SpokenFeedbackAppListBaseTest(SpokenFeedbackTestVariant variant)
      : variant_(variant) {}
  ~SpokenFeedbackAppListBaseTest() override = default;

  // LoggedInSpokenFeedbackTest:
  void SetUp() override {
    // Do not run expand arrow hinting animation to avoid msan test crash.
    // (See https://crbug.com/926038)
    zero_duration_mode_ =
        std::make_unique<gfx::ScopedAnimationDurationScaleMode>(
            gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    // Disable the app list nudge in the spoken feedback app list test.
    AppListTestApi().DisableAppListNudge(true);
    AppListControllerImpl::SetSunfishNudgeDisabledForTest(true);

    LoggedInSpokenFeedbackTest::SetUp();
  }

  void TearDown() override {
    LoggedInSpokenFeedbackTest::TearDown();
    zero_duration_mode_.reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoggedInSpokenFeedbackTest::SetUpCommandLine(command_line);

    if (variant_ == kTestAsGuestUser) {
      command_line->AppendSwitch(switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
      command_line->AppendSwitchASCII(
          switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
    }

    scoped_feature_list_.InitWithFeatures(
        {features::kFeatureManagementLocalImageSearch},
        {features::kScannerDogfood, features::kSunfishFeature});
  }

  void SetUpOnMainThread() override {
    LoggedInSpokenFeedbackTest::SetUpOnMainThread();
    AppListClientImpl::GetInstance()->UpdateProfile();
  }

  // Populate apps grid with |num| items.
  void PopulateApps(size_t num) {
    // Only folders or page breaks are allowed to be added from the Ash side.
    // Therefore new apps should be added through `ChromeAppListModelUpdater`.
    ::test::PopulateDummyAppListItems(num);
  }

  // Moves to the first test app in a populated list of apps.
  // Returns the index of that item.
  int MoveToFirstTestApp() {
    // Focus the shelf. This selects the launcher button.
    sm()->Call([this]() {
      EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
    });
    sm()->ExpectSpeechPattern("Launcher");
    sm()->ExpectSpeech("Button");
    sm()->ExpectSpeech("Shelf");
    sm()->ExpectSpeech("Tool bar");

    // Activate the launcher button. This opens bubble launcher.
    sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
    sm()->ExpectSpeechPattern("Search your *");
    sm()->ExpectSpeech("Edit text");

    sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
    sm()->ExpectSpeech("Button");

    int test_item_index = 0;
    AppListItem* test_item = FindItemByName("app 0", &test_item_index);
    EXPECT_TRUE(test_item);

    // Skip over apps that were installed before the test item.
    // This selects the first app installed by the test.
    for (int i = 0; i < test_item_index; ++i) {
      sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
    }
    sm()->ExpectSpeech("app 0");
    sm()->ExpectSpeech("Button");

    return test_item_index;
  }

  AppListItem* FindItemByName(const std::string& name, int* index) {
    AppListModel* const model = AppListModelProvider::Get()->model();
    AppListItemList* item_list = model->top_level_item_list();
    for (size_t i = 0; i < item_list->item_count(); ++i) {
      if (item_list->item_at(i)->name() == name) {
        if (index) {
          *index = i;
        }
        return item_list->item_at(i);
      }
    }
    return nullptr;
  }

  void ReadWindowTitle() {
    chromevox_test_utils()->ExecuteCommandHandlerCommand("readCurrentTitle");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  const SpokenFeedbackTestVariant variant_;
  std::unique_ptr<gfx::ScopedAnimationDurationScaleMode> zero_duration_mode_;
};

class SpokenFeedbackAppListTest : public SpokenFeedbackAppListBaseTest {
 public:
  SpokenFeedbackAppListTest()
      : SpokenFeedbackAppListBaseTest(GetParam().variant().value()) {}
  ~SpokenFeedbackAppListTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2NormalUser,
    SpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV2GuestUser,
    SpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsGuestUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3NormalUser,
    SpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3GuestUser,
    SpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsGuestUser)));

class NotificationSpokenFeedbackAppListTest : public SpokenFeedbackAppListTest {
 protected:
  NotificationSpokenFeedbackAppListTest() = default;
  ~NotificationSpokenFeedbackAppListTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SpokenFeedbackAppListTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAshEnableTabletMode);
  }
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2NormalUser,
    NotificationSpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV2GuestUser,
    NotificationSpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsGuestUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3NormalUser,
    NotificationSpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsNormalUser)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3GuestUser,
    NotificationSpokenFeedbackAppListTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsGuestUser)));

class SpokenFeedbackAppListSearchTest : public SpokenFeedbackAppListBaseTest {
 public:
  SpokenFeedbackAppListSearchTest()
      : SpokenFeedbackAppListBaseTest(GetParam().variant().value()),
        tablet_mode_(GetParam().tablet_mode().value()) {}
  ~SpokenFeedbackAppListSearchTest() override = default;

  // SpokenFeedbackAppListTest:
  void SetUpOnMainThread() override {
    SpokenFeedbackAppListBaseTest::SetUpOnMainThread();

    AppListClientImpl* app_list_client = AppListClientImpl::GetInstance();

    // Reset default search controller, so the test has better control over the
    // set of results shown in the search result UI.
    std::unique_ptr<app_list::SearchController> search_controller =
        std::make_unique<app_list::SearchController>(
            app_list_client->GetModelUpdaterForTest(), app_list_client, nullptr,
            browser()->profile());
    search_controller->Initialize();
    // Disable ranking, which may override the explicitly set relevance scores
    // and best match status of results.
    search_controller->disable_ranking_for_test();
    InitializeTestSearchProviders(search_controller.get(), &apps_provider_,
                                  &web_provider_, &image_provider_);
    ASSERT_TRUE(apps_provider_);
    ASSERT_TRUE(web_provider_);
    ASSERT_TRUE(image_provider_);
    app_list_client->SetSearchControllerForTest(std::move(search_controller));

    ShellTestApi().SetTabletModeEnabledForTest(tablet_mode_);
  }

  void TearDownOnMainThread() override {
    apps_provider_ = nullptr;
    web_provider_ = nullptr;
    image_provider_ = nullptr;
    AppListClientImpl::GetInstance()->SetSearchControllerForTest(nullptr);
    SpokenFeedbackAppListBaseTest::TearDownOnMainThread();
  }

  void ShowAppList() {
    if (tablet_mode_) {
      // Minimize the test window to transition to tablet mode home screen.
      sm()->Call([this]() { browser()->window()->Minimize(); });
    } else {
      // Focus the home button and press it to open the bubble launcher.
      sm()->Call([this]() {
        EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
      });
      sm()->ExpectSpeechPattern("Launcher");
      sm()->ExpectSpeech("Button");
      sm()->ExpectSpeech("Shelf");
      sm()->ExpectSpeech("Tool bar");

      sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
    }
  }

 protected:
  // Whether the test runs in tablet mode.
  const bool tablet_mode_;

  raw_ptr<TestSearchProvider> apps_provider_ = nullptr;
  raw_ptr<TestSearchProvider> web_provider_ = nullptr;
  raw_ptr<TestSearchProvider> image_provider_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    ManifestV2NormalUserTabletMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsNormalUser,
                                               /*tablet_mode=*/true)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV2GuestUserTabletMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsGuestUser,
                                               /*tablet_mode=*/true)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV2NormalUserDesktopMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsNormalUser,
                                               /*tablet_mode=*/false)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV2GuestUserDesktopMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kTwo,
                                               kTestAsGuestUser,
                                               /*tablet_mode=*/false)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3NormalUserTabletMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsNormalUser,
                                               /*tablet_mode=*/true)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3GuestUserTabletMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsGuestUser,
                                               /*tablet_mode=*/true)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3NormalUserDesktopMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsNormalUser,
                                               /*tablet_mode=*/false)));

INSTANTIATE_TEST_SUITE_P(
    ManifestV3GuestUserDesktopMode,
    SpokenFeedbackAppListSearchTest,
    ::testing::Values(SpokenFeedbackTestConfig(ManifestVersion::kThree,
                                               kTestAsGuestUser,
                                               /*tablet_mode=*/false)));

// Checks that when an app list item with a notification badge is focused, an
// announcement is made that the item requests your attention.
IN_PROC_BROWSER_TEST_P(NotificationSpokenFeedbackAppListTest,
                       AppListItemNotificationBadgeAnnounced) {
  PopulateApps(1);

  int test_item_index = 0;
  AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);
  test_item->UpdateNotificationBadgeForTesting(true);

  chromevox_test_utils()->EnableChromeVox();

  // Focus the shelf. This selects the launcher button.
  sm()->Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
  });
  sm()->ExpectSpeechPattern("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Activate the launcher button. This opens bubble launcher.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");

  // Skip over apps that were installed before the test item.
  sm()->Call([this, &test_item_index]() {
    for (int i = 0; i < test_item_index + 1; ++i) {
      SendKeyPressWithSearch(ui::VKEY_RIGHT);
    }
  });

  // Check that the announcement for items with a notification badge occurs.
  sm()->ExpectSpeech("app 0 requests your attention.");
  sm()->Replay();
}

// Checks that when a paused app list item is focused, an announcement 'Paused'
// is made.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       AppListItemPausedAppAnnounced) {
  PopulateApps(1);

  int test_item_index = 0;
  AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);
  test_item->UpdateAppStatusForTesting(AppStatus::kPaused);

  chromevox_test_utils()->EnableChromeVox();

  // Focus the shelf. This selects the launcher button.
  sm()->Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
  });
  sm()->ExpectSpeechPattern("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Activate the launcher button. This opens bubble launcher.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");

  // Skip over apps that were installed before the test item.
  sm()->Call([this, &test_item_index]() {
    for (int i = 0; i < test_item_index + 1; ++i) {
      SendKeyPressWithSearch(ui::VKEY_RIGHT);
    }
  });

  // Check that the announcement for items with a pause badge occurs.
  sm()->ExpectSpeech("app 0");
  sm()->ExpectSpeech("Paused");
  sm()->Replay();
}

// Checks that when a blocked app list item is focused, an announcement
// 'Blocked' is made.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       AppListItemBlockedAppAnnounced) {
  PopulateApps(1);

  int test_item_index = 0;
  AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);
  test_item->UpdateAppStatusForTesting(AppStatus::kBlocked);

  chromevox_test_utils()->EnableChromeVox();

  // Focus the shelf. This selects the launcher button.
  sm()->Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
  });
  sm()->ExpectSpeechPattern("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Activate the launcher button. This opens bubble launcher.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");

  // Skip over apps that were installed before the test item.
  sm()->Call([this, &test_item_index]() {
    for (int i = 0; i < test_item_index + 1; ++i) {
      SendKeyPressWithSearch(ui::VKEY_RIGHT);
    }
  });

  // Check that the announcement for items with a block badge occurs.
  sm()->ExpectSpeech("app 0");
  sm()->ExpectSpeech("Blocked");
  sm()->Replay();
}

// Checks that entering and exiting tablet mode with a browser window open does
// not generate an accessibility event.
IN_PROC_BROWSER_TEST_P(
    SpokenFeedbackAppListTest,
    HiddenAppListDoesNotCreateAccessibilityEventWhenTransitioningToTabletMode) {
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([]() { ShellTestApi().SetTabletModeEnabledForTest(true); });
  sm()->ExpectNextSpeechIsNot("Launcher, all apps");
  sm()->Call([]() { ShellTestApi().SetTabletModeEnabledForTest(false); });
  sm()->ExpectNextSpeechIsNot("Launcher, all apps");
  sm()->Replay();
}

// Checks that rotating the display in tablet mode does not generate an
// accessibility event.
IN_PROC_BROWSER_TEST_P(
    SpokenFeedbackAppListTest,
    LauncherAppListScreenRotationDoesNotCreateAccessibilityEvent) {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const int display_id = display_manager->GetDisplayAt(0).id();
  chromevox_test_utils()->EnableChromeVox();

  sm()->Call([]() { ShellTestApi().SetTabletModeEnabledForTest(true); });

  sm()->Call([this]() { browser()->window()->Minimize(); });
  // Set screen rotation to 90 degrees. No ChromeVox event should be created.
  sm()->Call([&, display_manager, display_id]() {
    display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_90,
                                        display::Display::RotationSource::USER);
  });
  sm()->ExpectNextSpeechIsNot("Launcher, all apps");

  // Set screen rotation to 0 degrees. No ChromeVox event should be created.
  sm()->Call([&, display_manager, display_id]() {
    display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_0,
                                        display::Display::RotationSource::USER);
  });
  sm()->ExpectNextSpeechIsNot("Launcher, all apps");

  sm()->Replay();
}

// TODO(https://crbug.com/1393235): Update this browser test to test recent
// apps.
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, ClamshellLauncher) {
  PopulateApps(3);

  int test_item_index = 0;
  AppListItem* test_item = FindItemByName("app 0", &test_item_index);
  ASSERT_TRUE(test_item);

  chromevox_test_utils()->EnableChromeVox();

  // Focus the shelf. This selects the launcher button.
  sm()->Call([this]() {
    EXPECT_TRUE(PerformAcceleratorAction(AcceleratorAction::kFocusShelf));
  });
  sm()->ExpectSpeechPattern("Launcher");
  sm()->ExpectSpeech("Button");
  sm()->ExpectSpeech("Shelf");
  sm()->ExpectSpeech("Tool bar");

  // Activate the launcher button. This opens bubble launcher.
  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_SPACE); });
  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");
  sm()->ExpectSpeech("Launcher, all apps");
  sm()->Call([this]() { ReadWindowTitle(); });
  sm()->ExpectSpeech("Launcher");

  sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  sm()->ExpectSpeech("Button");

  // Skip over apps that were installed before the test item.
  // This selects the first app installed by the test.
  for (int i = 0; i < test_item_index; ++i) {
    sm()->Call([this]() { SendKeyPressWithSearch(ui::VKEY_RIGHT); });
  }
  sm()->ExpectSpeech("app 0");
  sm()->ExpectSpeech("Button");

  // Move the focused item to the right. The announcement does not include a
  // page because the bubble launcher apps grid is scrollable, not paged.
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_RIGHT); });

  sm()->ExpectSpeech(
      base::StringPrintf("Moved to row 1, column %d.", test_item_index + 2));

  sm()->Replay();
}

// Checks that app list keyboard reordering is announced.
// TODO(mmourgos): The current method of accessibility announcements for item
// reordering uses alerts, this works for spoken feedback but does not work as
// well for braille users. The preferred way to handle this is to actually
// change focus as the user navigates, and to have each object's
// accessible name describe its position. (See crbug.com/1098495)
IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, AppListReordering) {
  PopulateApps(22);
  chromevox_test_utils()->EnableChromeVox();
  const int test_item_index = MoveToFirstTestApp();

  // The default column of app 0.
  const int original_column = test_item_index + 1;

  // The column of app 0 after rightward move.
  const int column_after_horizontal_move = original_column + 1;

  // Move the first item to the right.
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_RIGHT); });
  sm()->ExpectNextSpeechIsNot("Alert");

  sm()->ExpectSpeech(base::StringPrintf("Moved to row 1, column %d.",
                                        column_after_horizontal_move));

  // Move the focused item down.
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm()->ExpectNextSpeechIsNot("Alert");
  sm()->ExpectSpeech(base::StringPrintf("Moved to row 2, column %d.",
                                        column_after_horizontal_move));

  // Move the focused item down.
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm()->ExpectNextSpeechIsNot("Alert");
  sm()->ExpectSpeech(base::StringPrintf("Moved to row 3, column %d.",
                                        column_after_horizontal_move));

  // Move the focused item down.
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_DOWN); });
  sm()->ExpectNextSpeechIsNot("Alert");
  sm()->ExpectSpeech(base::StringPrintf("Moved to row 4, column %d.",
                                        column_after_horizontal_move));

  // Move the focused item left.
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_LEFT); });
  sm()->ExpectNextSpeechIsNot("Alert");
  sm()->ExpectSpeech(
      base::StringPrintf("Moved to row 4, column %d.", original_column));

  // Move the focused item back up.
  sm()->Call([this]() { SendKeyPressWithControl(ui::VKEY_UP); });
  sm()->ExpectNextSpeechIsNot("Alert");
  sm()->ExpectSpeech(
      base::StringPrintf("Moved to row 3, column %d.", original_column));

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest, AppListFoldering) {
  // Add 3 apps and move to the first one.
  PopulateApps(3);
  chromevox_test_utils()->EnableChromeVox();
  const int test_item_index = MoveToFirstTestApp();

  auto* generator_ptr = event_generator();
  // Combine items and create a new folder.
  sm()->Call([generator_ptr]() {
    SendKeyPressWithShiftAndControl(generator_ptr, ui::VKEY_RIGHT);
  });
  sm()->ExpectNextSpeechIsNot("Alert");
  sm()->ExpectSpeech("Folder Unnamed");
  sm()->ExpectSpeech("Expanded");
  sm()->ExpectSpeech("app 0 combined with app 1 to create new folder.");

  // Move focus to the first item in folder.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("app 1");
  sm()->ExpectSpeech("Button");

  // Remove the first item from the folder back to the top level app list.
  sm()->Call([generator_ptr]() {
    SendKeyPressWithShiftAndControl(generator_ptr, ui::VKEY_LEFT);
  });

  sm()->ExpectSpeech("app 1");
  sm()->ExpectSpeech("Button");
  sm()->ExpectNextSpeechIsNot("Alert");

  sm()->ExpectSpeech(
      base::StringPrintf("Moved to row 1, column %d.", test_item_index + 1));

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       CloseFolderMakesA11yAnnouncement) {
  // Add 3 apps and move to the first one.
  PopulateApps(3);
  chromevox_test_utils()->EnableChromeVox();
  MoveToFirstTestApp();

  // Combine items and create a new folder.
  auto* generator_ptr = event_generator();
  sm()->Call([generator_ptr]() {
    SendKeyPressWithShiftAndControl(generator_ptr, ui::VKEY_RIGHT);
  });
  sm()->ExpectNextSpeechIsNot("Alert");
  sm()->ExpectSpeech("Folder Unnamed");
  sm()->ExpectSpeech("Expanded");
  sm()->ExpectSpeech("app 0 combined with app 1 to create new folder.");

  // Move focus to the first item in folder.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("app 1");
  sm()->ExpectSpeech("Button");

  // Press Escape to close the folder. ChromeVox should announce that the
  // folder has been closed.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_ESCAPE); });
  sm()->ExpectSpeech("Close folder");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListTest,
                       SortAppsMakesA11yAnnouncement) {
  // Add 3 apps and move to the first one.
  PopulateApps(3);
  chromevox_test_utils()->EnableChromeVox();
  MoveToFirstTestApp();

  // The AppListTestApi's ReorderByMouseClickAtToplevelAppsGridMenu times out
  // when called with ReorderAnimationEndState::kCompleted due to the run loop
  // in WaitForReorderAnimationAndVerifyItemVisibility. So we do some of that
  // work here manually.

  // Show the context menu for the AppsGridView.
  sm()->Call([]() {
    AppsGridView* grid_view = AppListTestApi().GetTopLevelAppsGridView();
    EXPECT_TRUE(grid_view);
    grid_view->ShowContextMenu(grid_view->GetBoundsInScreen().CenterPoint(),
                               ui::mojom::MenuSourceType::kKeyboard);
  });
  sm()->ExpectSpeech("menu opened");

  // Press N to activate sort by name. This will result in focus being moved
  // to a button to undo the sort just completed.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_N); });
  sm()->ExpectSpeech("Apps are sorted by name");
  sm()->ExpectSpeech("Undo sort order by name");
  sm()->ExpectSpeech("Button");

  // Press the undo button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm()->ExpectSpeech("Sort undo successful");

  // Show the context menu for the AppsGridView.
  sm()->Call([]() {
    AppsGridView* grid_view = AppListTestApi().GetTopLevelAppsGridView();
    EXPECT_TRUE(grid_view);
    grid_view->ShowContextMenu(grid_view->GetBoundsInScreen().CenterPoint(),
                               ui::mojom::MenuSourceType::kKeyboard);
  });
  sm()->ExpectSpeech("menu opened");

  // Press C to activate sort by color. This will result in focus being moved
  // to a button to undo the sort just completed.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_C); });
  sm()->ExpectSpeech("Apps are sorted by color");
  sm()->ExpectSpeech("Undo sort order by color");
  sm()->ExpectSpeech("Button");

  // Press the undo button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_SPACE); });
  sm()->ExpectSpeech("Sort undo successful");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListSearchTest, LauncherSearch) {
  chromevox_test_utils()->EnableChromeVox();
  ShowAppList();

  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");
  sm()->Call([this]() { ReadWindowTitle(); });
  sm()->ExpectSpeech("Launcher");

  sm()->Call([this]() {
    apps_provider_->set_best_match_count(2);
    apps_provider_->set_count(3);
    web_provider_->set_count(4);
    image_provider_->set_count(3);
    SendKeyPress(ui::VKEY_G);
  });

  sm()->ExpectSpeech("G");
  sm()->ExpectSpeech("app 0");
  sm()->ExpectSpeech("List item 1 of 2");
  sm()->ExpectSpeech("Best Match");
  sm()->ExpectSpeech("List box");

  // Traverse best match results;
  for (int i = 1; i < 2; ++i) {
    sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
    sm()->ExpectSpeech(base::StringPrintf("app %d", i));
    sm()->ExpectSpeech(base::StringPrintf("List item %d of 2", i + 1));
  }

  // Traverse image results.
  for (int i = 0; i < 3; ++i) {
    sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
    sm()->ExpectSpeech(base::StringPrintf("image %d", i));
    sm()->ExpectSpeech(base::StringPrintf("List item %d of 3", i + 1));
    if (i == 0) {
      sm()->ExpectSpeech("List box");
    }
  }

  // Traverse non-best-match app results.
  for (int i = 2; i < 5; ++i) {
    sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
    sm()->ExpectSpeech(base::StringPrintf("app %d", i));
    sm()->ExpectSpeech(
        base::StringPrintf("List item %d of 3", (i - 2) % 3 + 1));
    if (i == 2) {
      sm()->ExpectSpeech("Apps");
      sm()->ExpectSpeech("List box");
    }
  }

  // Traverse omnibox results.
  for (int i = 0; i < 3; ++i) {
    sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
    sm()->ExpectSpeech(base::StringPrintf("item %d", i));
    sm()->ExpectSpeech(base::StringPrintf("List item %d of 3", i + 1));
    if (i == 0) {
      sm()->ExpectSpeech("Websites");
      sm()->ExpectSpeech("List box");
    }
  }

  // Cycle focus to the filter button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Toggle search result categories");

  // Move focus to the close button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Clear searchbox text");

  // Move focus back to the filter button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeech("Toggle search result categories");

  // Go back to the last result.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeech("item 2");

  // Update the query, to initiate new search.
  sm()->Call([this]() {
    apps_provider_->set_best_match_count(0);
    apps_provider_->set_count(3);
    web_provider_->set_count(2);
    image_provider_->set_count(2);
    SendKeyPress(ui::VKEY_A);
  });

  sm()->ExpectSpeech("A");
  sm()->ExpectSpeech("image 0");
  sm()->ExpectSpeech("List item 1 of 2");
  sm()->ExpectSpeech("List box");

  // Traverse image results.
  for (int i = 1; i < 2; ++i) {
    sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
    sm()->ExpectSpeech(base::StringPrintf("image %d", i));
    sm()->ExpectSpeech(base::StringPrintf("List item %d of 2", i + 1));
  }

  // Verify traversal works after result change.
  for (int i = 0; i < 3; ++i) {
    sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
    sm()->ExpectSpeech(base::StringPrintf("app %d", i));
    sm()->ExpectSpeech(base::StringPrintf("List item %d of 3", i + 1));
  }

  for (int i = 0; i < 2; ++i) {
    sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
    sm()->ExpectSpeech(base::StringPrintf("item %d", i));
    sm()->ExpectSpeech(base::StringPrintf("List item %d of 2", i + 1));
    if (i == 0) {
      sm()->ExpectSpeech("Websites");
      sm()->ExpectSpeech("List box");
    }
  }

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListSearchTest,
                       TouchExploreLauncherSearchResult) {
  chromevox_test_utils()->EnableChromeVox();
  ShowAppList();

  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() {
    apps_provider_->set_best_match_count(2);
    apps_provider_->set_count(3);
    web_provider_->set_count(4);
    SendKeyPress(ui::VKEY_G);
  });

  sm()->ExpectSpeech("G");
  sm()->ExpectSpeech("Displaying 8 results for g");

  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  auto* generator_ptr = event_generator();

  // Start touch exploration, and go to the third result in the UI (expected to
  // be "app 2").
  sm()->Call([clock_ptr, generator_ptr]() {
    views::View* target_view =
        AppListTestApi().GetVisibleSearchResultView(/*index=*/2);
    ASSERT_TRUE(target_view);

    gfx::Point touch_point = target_view->GetBoundsInScreen().CenterPoint();
    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, touch_point, base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, touch_point, base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);
  });

  // The result under touch pointer should be announced.
  sm()->ExpectSpeech("app 2");
  sm()->ExpectSpeech("List item 1 of 3");
  sm()->ExpectSpeech("Apps");
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListSearchTest, VocalizeResultCount) {
  chromevox_test_utils()->EnableChromeVox();
  ShowAppList();

  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() {
    apps_provider_->set_best_match_count(2);
    apps_provider_->set_count(3);
    web_provider_->set_count(4);
    SendKeyPress(ui::VKEY_G);
  });

  sm()->ExpectSpeech("G");
  sm()->ExpectSpeech("Displaying 8 results for g");

  // Update the query, to initiate new search.
  sm()->Call([this]() {
    apps_provider_->set_best_match_count(0);
    apps_provider_->set_count(3);
    web_provider_->set_count(2);
    SendKeyPress(ui::VKEY_A);
  });

  sm()->ExpectSpeech("A");
  sm()->ExpectSpeech("Displaying 5 results for ga");

  sm()->Replay();
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackAppListSearchTest, SearchCategoryFilter) {
  if (GetParam().manifest_version() == ManifestVersion::kThree) {
    // TODO(https://crbug.com/433771715): This test is failing on MSAN builds
    // when ChromeVox mv3 is enabled.
    return;
  }

  chromevox_test_utils()->EnableChromeVox();
  ShowAppList();

  sm()->ExpectSpeechPattern("Search your *");
  sm()->ExpectSpeech("Edit text");

  sm()->Call([this]() {
    apps_provider_->set_best_match_count(2);
    apps_provider_->set_count(3);
    web_provider_->set_count(4);
    SendKeyPress(ui::VKEY_G);
  });

  sm()->ExpectSpeech("G");
  sm()->ExpectSpeech("Displaying 8 results for g");

  // Move focus to the close button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeech("Clear searchbox text");

  // Move focus to the filter button.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_UP); });
  sm()->ExpectSpeech("Toggle search result categories");

  // Open the filter menu.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_RETURN); });
  sm()->ExpectSpeech("menu opened");

  // Move focus to the category options.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Apps");
  sm()->ExpectSpeech("Checked");
  sm()->ExpectSpeech("Your installed apps");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Images");
  sm()->ExpectSpeech("Checked");
  sm()->ExpectSpeech("Image search by content and image previews");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_DOWN); });
  sm()->ExpectSpeech("Websites");
  sm()->ExpectSpeech("Checked");
  sm()->ExpectSpeech("Websites including pages you've visited and open pages");

  // Toggle the websites search category.
  sm()->Call([this]() { SendKeyPress(ui::VKEY_RETURN); });
  sm()->ExpectSpeech("Websites");
  sm()->ExpectSpeech("Not checked");
  sm()->ExpectSpeech("Websites including pages you've visited and open pages");

  sm()->Call([this]() { SendKeyPress(ui::VKEY_ESCAPE); });
  sm()->Replay();
}

}  // namespace ash
