// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_client.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/switch.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/models/image_model.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// Returns the pref service to use for Birch bar prefs.
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Check if given suggestion types are shown in the bar chips.
bool HasSuggestionTypes(
    const std::vector<BirchItemType>& types,
    const std::vector<raw_ptr<BirchChipButtonBase>>& chips) {
  return base::ranges::all_of(types, [&](BirchItemType type) {
    return base::ranges::any_of(chips, [&](raw_ptr<BirchChipButtonBase> chip) {
      return chip->GetItem()->GetType() == type;
    });
  });
}

////////////////////////////////////////////////////////////////////////////////
// TestBirchItem:
class TestBirchItem : public BirchItem {
 public:
  TestBirchItem(const std::u16string& title,
                const std::u16string& subtitle,
                const std::optional<std::u16string>& secondary_action,
                float ranking = 1.0f)
      : BirchItem(title, subtitle) {
    set_ranking(ranking);
    if (secondary_action) {
      set_secondary_action(*secondary_action);
    }
  }
  TestBirchItem(const BirchItem&) = delete;
  const BirchItem& operator=(const BirchItem&) = delete;
  ~TestBirchItem() override = default;

  // BirchItem:
  BirchItemType GetType() const override { return BirchItemType::kTest; }
  std::string ToString() const override {
    return std::string("Test item ") + base::UTF16ToUTF8(title());
  }
  void PerformAction() override {}
  void PerformSecondaryAction() override {}
  void LoadIcon(LoadIconCallback callback) const override {
    std::move(callback).Run(
        ui::ImageModel::FromVectorIcon(kSettingsIcon, SK_ColorBLACK, 20));
  }
};

////////////////////////////////////////////////////////////////////////////////
// TestBirchDataProvider:
// A test birch data provider that runs the data fetched callback with saved
// items when receives a data fetch request.
template <typename T>
class TestBirchDataProvider : public BirchDataProvider {
 public:
  using DataFetchedCallback =
      base::RepeatingCallback<void(const std::vector<T>&)>;

  TestBirchDataProvider(DataFetchedCallback data_fetched_callback,
                        const std::string& pref_name)
      : data_fetched_callback_(data_fetched_callback), pref_name_(pref_name) {}
  TestBirchDataProvider(const TestBirchDataProvider&) = delete;
  TestBirchDataProvider& operator=(const TestBirchDataProvider&) = delete;
  ~TestBirchDataProvider() override = default;

  void set_items(const std::vector<T>& items) { items_ = items; }

  void ClearItems() { items_.clear(); }

  // BirchDataProvider:
  void RequestBirchDataFetch() override {
    data_fetched_callback_.Run(
        (pref_name_.empty() || GetPrefService()->GetBoolean(pref_name_))
            ? items_
            : std::vector<T>());
  }

 private:
  DataFetchedCallback data_fetched_callback_;
  const std::string pref_name_;
  std::vector<T> items_;
};

////////////////////////////////////////////////////////////////////////////////
// TestBirchClient:
// A test birch client that returns the specific items to birch model.
class TestBirchClient : public BirchClient {
 public:
  explicit TestBirchClient(BirchModel* birch_model) {
    calendar_provider_ =
        std::make_unique<TestBirchDataProvider<BirchCalendarItem>>(
            base::BindRepeating(&TestBirchClient::HandleCalendarFetch,
                                base::Unretained(this)),
            prefs::kBirchUseCalendar);
    file_provider_ = std::make_unique<TestBirchDataProvider<BirchFileItem>>(
        base::BindRepeating(&BirchModel::SetFileSuggestItems,
                            base::Unretained(birch_model)),
        prefs::kBirchUseFileSuggest);
    tab_provider_ = std::make_unique<TestBirchDataProvider<BirchTabItem>>(
        base::BindRepeating(&BirchModel::SetRecentTabItems,
                            base::Unretained(birch_model)),
        prefs::kBirchUseRecentTabs);
    last_active_provider_ =
        std::make_unique<TestBirchDataProvider<BirchLastActiveItem>>(
            base::BindRepeating(&BirchModel::SetLastActiveItems,
                                base::Unretained(birch_model)),
            prefs::kBirchUseLastActive);
    most_visited_provider_ =
        std::make_unique<TestBirchDataProvider<BirchMostVisitedItem>>(
            base::BindRepeating(&BirchModel::SetMostVisitedItems,
                                base::Unretained(birch_model)),
            prefs::kBirchUseMostVisited);
    self_share_provider_ =
        std::make_unique<TestBirchDataProvider<BirchSelfShareItem>>(
            base::BindRepeating(&BirchModel::SetSelfShareItems,
                                base::Unretained(birch_model)),
            prefs::kBirchUseSelfShare);
    release_notes_provider_ =
        std::make_unique<TestBirchDataProvider<BirchReleaseNotesItem>>(
            base::BindRepeating(&BirchModel::SetReleaseNotesItems,
                                base::Unretained(birch_model)),
            std::string());
    if (features::IsBirchWeatherV2Enabled()) {
      weather_provider_ =
          std::make_unique<TestBirchDataProvider<BirchWeatherItem>>(
              base::BindRepeating(&BirchModel::SetWeatherItems,
                                  base::Unretained(birch_model)),
              prefs::kBirchUseWeather);
    }
    EXPECT_TRUE(test_dir_.CreateUniqueTempDir());
  }
  TestBirchClient(const TestBirchClient&) = delete;
  TestBirchClient& operator=(const TestBirchClient&) = delete;
  ~TestBirchClient() override = default;

  void SetCalendarItems(const std::vector<BirchCalendarItem>& items) {
    calendar_provider_->set_items(items);
  }

  void SetFileSuggestItems(const std::vector<BirchFileItem>& items) {
    file_provider_->set_items(items);
  }

  void SetRecentTabsItems(const std::vector<BirchTabItem>& items) {
    tab_provider_->set_items(items);
  }

  void SetLastActiveItems(const std::vector<BirchLastActiveItem>& items) {
    last_active_provider_->set_items(items);
  }

  void SetMostVisitedItems(const std::vector<BirchMostVisitedItem>& items) {
    most_visited_provider_->set_items(items);
  }

  void SetReleaseNotesItems(const std::vector<BirchReleaseNotesItem>& items) {
    release_notes_provider_->set_items(items);
  }

  void SetSelfShareItems(const std::vector<BirchSelfShareItem>& items) {
    self_share_provider_->set_items(items);
  }

  void SetWeatherItems(const std::vector<BirchWeatherItem>& items) {
    ASSERT_TRUE(weather_provider_);
    weather_provider_->set_items(items);
  }

  // Clear all items.
  void Reset() {
    calendar_provider_->ClearItems();
    file_provider_->ClearItems();
    tab_provider_->ClearItems();
    last_active_provider_->ClearItems();
    release_notes_provider_->ClearItems();
    self_share_provider_->ClearItems();
    if (weather_provider_) {
      weather_provider_->ClearItems();
    }
  }

  // BirchClient:
  BirchDataProvider* GetCalendarProvider() override {
    return calendar_provider_.get();
  }
  BirchDataProvider* GetFileSuggestProvider() override {
    return file_provider_.get();
  }
  BirchDataProvider* GetRecentTabsProvider() override {
    return tab_provider_.get();
  }
  BirchDataProvider* GetLastActiveProvider() override {
    return last_active_provider_.get();
  }
  BirchDataProvider* GetMostVisitedProvider() override {
    return most_visited_provider_.get();
  }
  BirchDataProvider* GetSelfShareProvider() override {
    return self_share_provider_.get();
  }
  BirchDataProvider* GetReleaseNotesProvider() override {
    return release_notes_provider_.get();
  }
  BirchDataProvider* GetWeatherV2Provider() override {
    return weather_provider_.get();
  }

  void WaitForRefreshTokens(base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  base::FilePath GetRemovedItemsFilePath() override {
    return test_dir_.GetPath();
  }

 private:
  void HandleCalendarFetch(const std::vector<BirchCalendarItem>& items) {
    // The production calendar provider sets both calendar items and attachment
    // items. Set both so the fetch can complete.
    Shell::Get()->birch_model()->SetCalendarItems(items);
    Shell::Get()->birch_model()->SetAttachmentItems({});
  }

  std::unique_ptr<TestBirchDataProvider<BirchCalendarItem>> calendar_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchFileItem>> file_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchTabItem>> tab_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchLastActiveItem>>
      last_active_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchMostVisitedItem>>
      most_visited_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchSelfShareItem>>
      self_share_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchReleaseNotesItem>>
      release_notes_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchWeatherItem>> weather_provider_;
  base::ScopedTempDir test_dir_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BirchBarBaseTest:
// The test class of birch bar with Forest feature enabled by default.
class BirchBarTestBase : public AshTestBase {
 public:
  BirchBarTestBase(bool use_weather_v2_provider)
      : use_weather_v2_provider_(use_weather_v2_provider) {
    if (use_weather_v2_provider) {
      feature_list_.InitWithFeatures(
          {features::kForestFeature, features::kBirchWeather,
           features::kBirchWeatherV2},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {features::kForestFeature, features::kBirchWeather},
          {features::kBirchWeatherV2});
    }
  }

  BirchBarTestBase(const BirchBarTestBase&) = delete;
  BirchBarTestBase& operator=(const BirchBarTestBase&) = delete;
  ~BirchBarTestBase() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    image_downloader_ = std::make_unique<ash::TestImageDownloader>();

    // Set prefs of all suggestion types and show suggestions enabled.
    for (const auto& pref_name :
         {prefs::kBirchShowSuggestions, prefs::kBirchUseCalendar,
          prefs::kBirchUseWeather, prefs::kBirchUseFileSuggest,
          prefs::kBirchUseRecentTabs, prefs::kBirchUseReleaseNotes,
          prefs::kBirchUseSelfShare}) {
      GetPrefService()->SetBoolean(pref_name, true);
    }

    // Create test birch client and weather provider.
    auto* birch_model = Shell::Get()->birch_model();
    birch_client_ = std::make_unique<TestBirchClient>(birch_model);
    birch_model->SetClientAndInit(birch_client_.get());
    if (!use_weather_v2_provider_) {
      auto weather_provider =
          std::make_unique<TestBirchDataProvider<BirchWeatherItem>>(
              base::BindRepeating(&BirchModel::SetWeatherItems,
                                  base::Unretained(birch_model)),
              prefs::kBirchUseWeather);
      weather_provider_ = weather_provider.get();
      birch_model->OverrideWeatherProviderForTest(std::move(weather_provider));
    }
    base::RunLoop run_loop;
    Shell::Get()
        ->birch_model()
        ->GetItemRemoverForTest()
        ->SetProtoInitCallbackForTest(run_loop.QuitClosure());
    run_loop.Run();

    // Prepare a file item for test.
    SetFileItems(/*num=*/1);
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    weather_provider_ = nullptr;
    birch_client_.reset();
    image_downloader_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<TestImageDownloader> image_downloader_;

 protected:
  // Adds a number of `num` file birch items to data source.
  void SetFileItems(size_t num) {
    std::vector<BirchFileItem> item_list;
    for (size_t i = 0; i < num; i++) {
      item_list.emplace_back(
          /*file_path=*/base::FilePath(base::StringPrintf("test path %lu", i)),
          /*justification=*/u"suggestion",
          /*timestamp=*/base::Time(),
          /*file_id=*/base::StringPrintf("file_id_%lu", i),
          /*icon_url=*/"icon_url");
      item_list.back().set_ranking(1.0f);
    }
    birch_client_->SetFileSuggestItems(item_list);
  }

  // Adds a number of `num` calendar birch items to data source.
  void SetCalendarItems(size_t num) {
    std::vector<BirchCalendarItem> item_list;
    for (size_t i = 0; i < num; i++) {
      item_list.emplace_back(
          /*title=*/u"Event " + base::NumberToString16(i),
          /*start_time=*/base::Time(),
          /*end_time=*/base::Time(),
          /*calendar_url=*/GURL(),
          /*conference_url=*/GURL(),
          /*event_id=*/base::StringPrintf("event_id_%ld", i),
          /*all_day_event=*/false);
      item_list.back().set_ranking(1.0f);
    }
    birch_client_->SetCalendarItems(item_list);
  }

  // Adds a number of `num` tab birch items to data source.
  void SetTabItems(size_t num) {
    std::vector<BirchTabItem> item_list;
    for (size_t i = 0; i < num; i++) {
      item_list.emplace_back(
          /*title=*/u"tab", /*url*/ GURL("https://www.example.com/"),
          /*timestamp=*/base::Time(),
          /*favicon_url=*/GURL("https://www.favicon.com/"),
          /*session_name=*/"session",
          /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
      item_list.back().set_ranking(1.0f);
    }
    birch_client_->SetRecentTabsItems(item_list);
  }

  // Adds `num` last active birch items to data source.
  void SetLastActiveItems(size_t num) {
    std::vector<BirchLastActiveItem> item_list;
    for (size_t i = 0; i < num; ++i) {
      item_list.emplace_back(u"last active", GURL("https://yahoo.com/"),
                             base::Time(), ui::ImageModel());
      item_list.back().set_ranking(1.0f);
    }
    birch_client_->SetLastActiveItems(item_list);
  }

  // Adds `num` most visited birch items to data source.
  void SetMostVisitedItems(size_t num) {
    std::vector<BirchMostVisitedItem> item_list;
    for (size_t i = 0; i < num; ++i) {
      item_list.emplace_back(u"most visited", GURL("https://google.com/"),
                             ui::ImageModel());
      item_list.back().set_ranking(1.0f);
    }
    birch_client_->SetMostVisitedItems(item_list);
  }

  // Adds a number of `num` self share birch items to data source.
  GURL faviconUrl = GURL("https://www.favicon.com/");
  void SetSelfShareItems(size_t num) {
    std::vector<BirchSelfShareItem> item_list;
    for (size_t i = 0; i < num; i++) {
      item_list.emplace_back(
          /*guid=*/u"self share guid", /*title*/ u"self share tab",
          /*url=*/GURL("https://www.exampletwo.com/"),
          /*shared_time=*/base::Time(), /*device_name=*/u"my device",
          /*favicon_url=*/faviconUrl,
          /*activation_callback=*/base::DoNothing());
      item_list.back().set_ranking(1.0f);
    }
    birch_client_->SetSelfShareItems(item_list);
  }

  // Adds a number of `num` release notes birch items to data source.
  void SetReleaseNotesItems(size_t num) {
    std::vector<BirchReleaseNotesItem> item_list;
    for (size_t i = 0; i < num; i++) {
      item_list.emplace_back(/*release_notes_title=*/u"note",
                             /*release_notes_text=*/u"explore",
                             /*url=*/GURL("https://www.example.com/"),
                             /*first_seen=*/base::Time());
      item_list.back().set_ranking(1.0f);
    }
    birch_client_->SetReleaseNotesItems(item_list);
  }

  // Adds a number of `num` weather birch items to data source.
  void SetWeatherItems(size_t num) {
    std::vector<BirchWeatherItem> item_list;
    for (size_t i = 0; i < num; i++) {
      item_list.emplace_back(/*weather_description=*/u"cloudy",
                             /*temperature=*/72.f,
                             /*icon*/ ui::ImageModel());
      item_list.back().set_ranking(1.0f);
    }
    if (use_weather_v2_provider_) {
      birch_client_->SetWeatherItems(item_list);
    } else {
      weather_provider_->set_items(item_list);
    }
  }

  std::unique_ptr<TestBirchClient> birch_client_;
  raw_ptr<TestBirchDataProvider<BirchWeatherItem>> weather_provider_;

 private:
  const bool use_weather_v2_provider_;

  base::test::ScopedFeatureList feature_list_;
  // Ensure base::Time::Now() is a fixed value.
  base::ScopedMockClockOverride mock_clock_override_;
};

class BirchBarTest : public BirchBarTestBase,
                     public testing::WithParamInterface<bool> {
 public:
  BirchBarTest() : BirchBarTestBase(/*use_weather_v2_provider=*/GetParam()) {}
  BirchBarTest(const BirchBarTest&) = delete;
  BirchBarTest& operator=(const BirchBarTest&) = delete;
  ~BirchBarTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(UsingWeatherV2Provider, BirchBarTest, testing::Bool());

// Tests that the birch bar will be shown in the normal Overview.
TEST_P(BirchBarTest, ShowBirchBar) {
  EnterOverview();
  EXPECT_TRUE(
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).birch_bar_view());
}

TEST_P(BirchBarTest, DoNotShowBirchBarForSecondaryUser) {
  // Sign in a secondary user.
  SimulateUserLogin("user2@test.com");
  ASSERT_FALSE(Shell::Get()->session_controller()->IsUserPrimary());

  EnterOverview();
  EXPECT_FALSE(
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).birch_bar_view());
}

TEST_P(BirchBarTest, RecordsHistogramWhenChipsShown) {
  // Ensure a consistent timezone for this test.
  calendar_test_utils::ScopedLibcTimeZone scoped_timezone(
      "America/Los_Angeles");

  base::HistogramTester histograms;

  // Add an ongoing calendar event at the current time. This will create a
  // suggestion chip. Note that SetUp() also adds a file item.
  std::vector<BirchCalendarItem> items;
  items.emplace_back(u"Event", base::Time::Now() - base::Minutes(30),
                     base::Time::Now() + base::Minutes(30), GURL(), GURL(),
                     std::string(), /*all_day_event=*/false);
  birch_client_->SetCalendarItems(items);

  // Entering overview shows the birch bar.
  EnterOverview();
  base::RunLoop().RunUntilIdle();  // Wait for data fetch callback.

  // One impression was recorded for the birch bar.
  histograms.ExpectBucketCount("Ash.Birch.Bar.Impression", true, 1);

  // Two chips were shown.
  histograms.ExpectBucketCount("Ash.Birch.ChipCount", 2, 1);

  // One impression was recorded for each chip type.
  histograms.ExpectBucketCount("Ash.Birch.Chip.Impression",
                               BirchItemType::kFile, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Impression",
                               BirchItemType::kCalendar, 1);

  // Two rankings were recorded for the current time slot histogram.
  histograms.ExpectBucketCount("Ash.Birch.Ranking.1200to1700", 1, 1);
  histograms.ExpectBucketCount("Ash.Birch.Ranking.1200to1700", 12, 1);

  // The same ranking were recorded for the all-day total histogram.
  histograms.ExpectBucketCount("Ash.Birch.Ranking.Total", 1, 1);
  histograms.ExpectBucketCount("Ash.Birch.Ranking.Total", 12, 1);
}

// Tests that the birch bar will be hidden in the partial Overview with a split
// screen.
TEST_P(BirchBarTest, HideBirchBarInPartialSplitScreen) {
  // Create two windows.
  auto window_1 = CreateAppWindow(gfx::Rect(100, 100));
  // Need another window to keep partial Overview when `window_1` is snapped in
  // Overview session.
  auto window_2 = CreateAppWindow(gfx::Rect(100, 200));

  EnterOverview();

  // The birch bar should be shown in the normal Overview.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);
  EXPECT_TRUE(grid_test_api.birch_bar_view());

  // Snap window 1 to create a split screen and the birch bar should be hidden.
  SplitViewController::Get(root_window)
      ->SnapWindow(window_1.get(), SnapPosition::kPrimary);
  EXPECT_FALSE(grid_test_api.birch_bar_view());

  // Dismiss the split screen, the birch bar should be shown.
  window_1.reset();
  EXPECT_TRUE(grid_test_api.birch_bar_view());
}

TEST_P(BirchBarTest, ShowBirchBarInTabletMode) {
  EnterOverview();
  // Convert to Tablet mode, the birch bar should be shown in Overview mode.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);

  EnterOverview();
  EXPECT_TRUE(
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).birch_bar_view());
}

////////////////////////////////////////////////////////////////////////////////
// BirchBarMenuTest:
// The test class of birch bar context menu.
class BirchBarMenuTest : public BirchBarTestBase,
                         public testing::WithParamInterface<bool> {
 public:
  BirchBarMenuTest()
      : BirchBarTestBase(/*use_weather_v2_provider=*/GetParam()) {}
  BirchBarMenuTest(const BirchBarMenuTest&) = delete;
  BirchBarMenuTest& operator=(const BirchBarMenuTest&) = delete;
  ~BirchBarMenuTest() override = default;

  // BirchBarTest:
  void SetUp() override {
    BirchBarTestBase::SetUp();
    // Clear existing items.
    birch_client_->Reset();
    // Ensure screen is large enough to be able to click on all menu items.
    UpdateDisplay("1600x1200");
  }

 protected:
  BirchBarMenuModelAdapter* GetBirchBarChipMenuModelAdaper() {
    auto* overview_session = GetOverviewSession();
    CHECK(overview_session);
    return overview_session->birch_bar_controller()
        ->chip_menu_model_adapter_.get();
  }

  const std::vector<std::unique_ptr<BirchItem>>& GetBirchItemsInController()
      const {
    auto* overview_session = GetOverviewSession();
    CHECK(overview_session);
    return overview_session->birch_bar_controller()->items_;
  }
};

INSTANTIATE_TEST_SUITE_P(UsingWeatherV2Provider,
                         BirchBarMenuTest,
                         testing::Bool());

// Tests that removing a suggestion from context menu.
TEST_P(BirchBarMenuTest, RemoveChip) {
  // Create 5 suggestions with different item types.
  SetWeatherItems(/*num=*/1);
  SetCalendarItems(/*num=*/2);
  SetFileItems(/*num=*/1);
  SetTabItems(/*num=*/1);

  // Add another screen such that we will have two synchronized bar views.
  UpdateDisplay("1000x800,1000x800");

  // Enter Overview and check the two bar views are created.
  EnterOverview();

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(root_windows.size(), 2u);
  auto* root_window_1 = root_windows[0].get();
  auto grid_test_api_1 = OverviewGridTestApi(root_window_1);
  auto* root_window_2 = root_windows[1].get();
  auto grid_test_api_2 = OverviewGridTestApi(root_window_2);

  EXPECT_TRUE(grid_test_api_1.birch_bar_view());
  EXPECT_TRUE(grid_test_api_2.birch_bar_view());

  // Cache the chips on two bar views.
  const auto& bar_1_chips = grid_test_api_1.GetBirchChips();
  const auto& bar_2_chips = grid_test_api_2.GetBirchChips();

  // Cached items in birch bar controller.
  const auto& cached_items = GetBirchItemsInController();

  // A functor to check if the current number of items are as expected and the
  // chips showing on the bar match the items
  auto chips_match_items = [&](size_t expected_num) {
    for (const auto& chips : {bar_1_chips, bar_2_chips}) {
      size_t item_num = cached_items.size();
      EXPECT_EQ(item_num, expected_num);
      EXPECT_EQ(
          chips.size(),
          std::min(static_cast<size_t>(BirchBarView::kMaxChipsNum), item_num));
      for (size_t i = 0; i < chips.size(); i++) {
        EXPECT_EQ(chips[i]->GetItem(), cached_items[i].get());
      }
    }
  };

  // Initially, we should have all 5 items in controller.
  chips_match_items(5);

  // Remove the second chip on the first bar view.
  // Right clicking on the second chip of first bar view to show the context
  // menu.
  RightClickOn(bar_1_chips[2]);

  auto* model_adapter = GetBirchBarChipMenuModelAdaper();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  // Hiding the second suggestion by selecting the corresponding menu item.
  const auto* hide_suggestion_item =
      model_adapter->root_for_testing()->GetSubmenu()->GetMenuItemAt(0);
  EXPECT_EQ(hide_suggestion_item->GetCommand(),
            base::to_underlying(
                BirchChipContextMenuModel::CommandId::kHideSuggestion));

  LeftClickOn(hide_suggestion_item);

  // Check if the item is removed and the chips on both bars get updated.
  chips_match_items(4);

  // Remove the second chips on the second bar.
  RightClickOn(bar_2_chips[2]);

  model_adapter = GetBirchBarChipMenuModelAdaper();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  hide_suggestion_item =
      model_adapter->root_for_testing()->GetSubmenu()->GetMenuItemAt(0);

  LeftClickOn(hide_suggestion_item);
  // Check if the item is removed and the chips on both bars get updated.
  chips_match_items(3);
}

// Tests showing/hiding suggestions from context menu.
TEST_P(BirchBarMenuTest, ShowHideBar) {
  // Create a suggestion for test.
  SetFileItems(/*num=*/1);

  // Add another screen such that we will have two synchronized bar views.
  UpdateDisplay("1000x800,1000x800");

  // Enter Overview and check the two bar views are created.
  EnterOverview();

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(root_windows.size(), 2u);
  auto* root_window_1 = root_windows[0].get();
  auto grid_test_api_1 = std::make_unique<OverviewGridTestApi>(root_window_1);
  auto* root_window_2 = root_windows[1].get();
  auto grid_test_api_2 = std::make_unique<OverviewGridTestApi>(root_window_2);

  // The birch bars should be shown on both displays.
  EXPECT_TRUE(grid_test_api_1->birch_bar_view());
  EXPECT_TRUE(grid_test_api_2->birch_bar_view());

  auto* root_window_controller_1 =
      RootWindowController::ForWindow(root_window_1);
  // Right clicking on the wallpaper of the first display to show the context
  // menu.
  RightClickOn(root_window_controller_1->wallpaper_widget_controller()
                   ->GetWidget()
                   ->GetContentsView());

  auto* model_adapter =
      root_window_controller_1->menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  // Hiding the suggestions by clicking to the switch button.
  auto* hide_suggestions_item =
      model_adapter->root_for_testing()->GetSubmenu()->GetMenuItemAt(0);
  Switch* switch_button =
      AsViewClass<Switch>(hide_suggestions_item->children()[0]);
  EXPECT_TRUE(!!switch_button);
  EXPECT_TRUE(switch_button->GetIsOn());

  // Toggle the switch button to hide the suggestions.
  LeftClickOn(switch_button);

  // The birch bars should be hidden on both displays.
  EXPECT_FALSE(grid_test_api_1->birch_bar_view());
  EXPECT_FALSE(grid_test_api_2->birch_bar_view());

  // Exit and re-enter Overview. The birch bars should be hidden.
  grid_test_api_1.reset();
  grid_test_api_2.reset();
  ExitOverview();
  EnterOverview();

  grid_test_api_1 = std::make_unique<OverviewGridTestApi>(root_window_1);
  grid_test_api_2 = std::make_unique<OverviewGridTestApi>(root_window_2);

  // The birch bars should be shown on both displays.
  EXPECT_FALSE(grid_test_api_1->birch_bar_view());
  EXPECT_FALSE(grid_test_api_2->birch_bar_view());

  auto* root_window_controller_2 =
      RootWindowController::ForWindow(root_window_1);
  // Right clicking on the wallpaper of the second display to show the context
  // menu.
  RightClickOn(root_window_controller_2->wallpaper_widget_controller()
                   ->GetWidget()
                   ->GetContentsView());

  model_adapter = root_window_controller_2->menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  // Showing the suggestions by clicking to the switch button.
  hide_suggestions_item =
      model_adapter->root_for_testing()->GetSubmenu()->GetMenuItemAt(0);
  switch_button = AsViewClass<Switch>(hide_suggestions_item->children()[0]);
  EXPECT_FALSE(switch_button->GetIsOn());

  // Toggle the switch button to show the suggestions.
  LeftClickOn(switch_button);

  // The birch bars should be hidden on both displays.
  EXPECT_TRUE(grid_test_api_1->birch_bar_view());
  EXPECT_TRUE(grid_test_api_2->birch_bar_view());
}

// Tests customizing suggestions from context menu.
TEST_P(BirchBarMenuTest, CustomizeSuggestions) {
  // Create 4 suggestions, as the bar shows a maximum of 4 chips.
  SetWeatherItems(/*num=*/1);
  SetCalendarItems(/*num=*/1);
  SetFileItems(/*num=*/1);
  SetTabItems(/*num=*/1);

  // Set show suggestions initially.
  GetPrefService()->SetBoolean(prefs::kBirchShowSuggestions, true);

  // Enter Overview and check a bar view is created.
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);

  // The birch bars should be shown.
  EXPECT_TRUE(grid_test_api.birch_bar_view());

  // Cache the chips.
  const auto& bar_chips = grid_test_api.GetBirchChips();

  // At the beginning, all types should be shown on the bar.
  EXPECT_TRUE(
      HasSuggestionTypes({BirchItemType::kWeather, BirchItemType::kCalendar,
                          BirchItemType::kFile, BirchItemType::kTab},
                         bar_chips));

  auto* root_window_controller = RootWindowController::ForWindow(root_window);
  // Right clicking on the wallpaper of the first display to show the context
  // menu.
  RightClickOn(root_window_controller->wallpaper_widget_controller()
                   ->GetWidget()
                   ->GetContentsView());

  auto* model_adapter =
      root_window_controller->menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  base::flat_map<BirchItemType, views::MenuItemView*> type_to_item;
  auto* sub_menu = model_adapter->root_for_testing()->GetSubmenu();
  auto* weather_item = sub_menu->GetMenuItemAt(1);
  EXPECT_EQ(weather_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kWeatherSuggestions));
  type_to_item[BirchItemType::kWeather] = weather_item;

  auto* calendar_item = sub_menu->GetMenuItemAt(2);
  EXPECT_EQ(calendar_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kCalendarSuggestions));
  type_to_item[BirchItemType::kCalendar] = calendar_item;

  auto* file_item = sub_menu->GetMenuItemAt(3);
  EXPECT_EQ(file_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kDriveSuggestions));
  type_to_item[BirchItemType::kFile] = file_item;

  auto* tab_item = sub_menu->GetMenuItemAt(4);
  EXPECT_EQ(tab_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kOtherDeviceSuggestions));
  type_to_item[BirchItemType::kTab] = tab_item;

  // Deselect all types of suggestions one by one.
  for (auto type : {BirchItemType::kWeather, BirchItemType::kCalendar,
                    BirchItemType::kFile, BirchItemType::kTab}) {
    LeftClickOn(type_to_item[type]);
    EXPECT_FALSE(HasSuggestionTypes({type}, bar_chips));
  }

  // There is no suggestions showing on the bar.
  EXPECT_TRUE(bar_chips.empty());

  // Re-select all types of suggestions one by one.
  std::vector<BirchItemType> new_types;
  for (auto type : {BirchItemType::kWeather, BirchItemType::kCalendar,
                    BirchItemType::kFile, BirchItemType::kTab}) {
    LeftClickOn(type_to_item[type]);
    EXPECT_TRUE(HasSuggestionTypes(new_types, bar_chips));
  }
}

// The bar shows a maximum of 4 suggestion chips. The above test verifies
// customizing the first 4 suggestion types; this test verifies the rest.
TEST_P(BirchBarMenuTest, CustomizeSuggestionsExtended) {
  SetLastActiveItems(/*num=*/1);
  SetMostVisitedItems(/*num=*/1);

  // Set show suggestions initially.
  GetPrefService()->SetBoolean(prefs::kBirchShowSuggestions, true);

  // Enter Overview and check a bar view is created.
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);
  const auto& bar_chips = grid_test_api.GetBirchChips();

  // At the beginning, all types should be shown on the bar.
  EXPECT_TRUE(HasSuggestionTypes(
      {BirchItemType::kLastActive, BirchItemType::kMostVisited}, bar_chips));

  auto* root_window_controller = RootWindowController::ForWindow(root_window);
  // Right clicking on the wallpaper of the first display to show the context
  // menu.
  RightClickOn(root_window_controller->wallpaper_widget_controller()
                   ->GetWidget()
                   ->GetContentsView());
  auto* model_adapter =
      root_window_controller->menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  base::flat_map<BirchItemType, views::MenuItemView*> type_to_item;
  auto* sub_menu = model_adapter->root_for_testing()->GetSubmenu();
  auto* last_active_item = sub_menu->GetMenuItemAt(5);
  EXPECT_EQ(last_active_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kLastActiveSuggestions));
  type_to_item[BirchItemType::kLastActive] = last_active_item;

  auto* most_visited_item = sub_menu->GetMenuItemAt(6);
  EXPECT_EQ(most_visited_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kMostVisitedSuggestions));
  type_to_item[BirchItemType::kMostVisited] = most_visited_item;

  // Deselect all types of suggestions one by one.
  for (auto type : {BirchItemType::kLastActive, BirchItemType::kMostVisited}) {
    LeftClickOn(type_to_item[type]);
    EXPECT_FALSE(HasSuggestionTypes({type}, bar_chips));
  }

  // There is no suggestions showing on the bar.
  EXPECT_TRUE(bar_chips.empty());

  // Re-select all types of suggestions one by one.
  std::vector<BirchItemType> new_types;
  for (auto type : {BirchItemType::kLastActive, BirchItemType::kMostVisited}) {
    LeftClickOn(type_to_item[type]);
    EXPECT_TRUE(HasSuggestionTypes(new_types, bar_chips));
  }
}

// Tests resetting suggestions from context menu.
TEST_P(BirchBarMenuTest, ResetSuggestions) {
  // Create 4 suggestions, one for each customizable suggestion type.
  SetCalendarItems(/*num=*/1);
  SetFileItems(/*num=*/1);
  SetTabItems(/*num=*/1);
  SetSelfShareItems(/*num*/ 1);

  // Enter Overview and check a bar view is created.
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);

  // The birch bars should be shown.
  EXPECT_TRUE(grid_test_api.birch_bar_view());

  // Cache the chips.
  const auto& bar_chips = grid_test_api.GetBirchChips();

  // The functor to check if given suggestion types are shown in the bar chips.
  auto has_suggestion_types =
      [](const std::vector<BirchItemType>& types,
         const std::vector<raw_ptr<BirchChipButtonBase>>& chips) -> bool {
    return base::ranges::all_of(types, [&](BirchItemType type) {
      return base::ranges::any_of(chips,
                                  [&](raw_ptr<BirchChipButtonBase> chip) {
                                    return chip->GetItem()->GetType() == type;
                                  });
    });
  };

  // Disable the calendar and file suggestions such that only tab and
  // self share suggestions are shown.
  auto* pref_service = GetPrefService();
  pref_service->SetBoolean(prefs::kBirchUseCalendar, false);
  pref_service->SetBoolean(prefs::kBirchUseFileSuggest, false);

  EXPECT_EQ(2u, bar_chips.size());
  EXPECT_TRUE(has_suggestion_types(
      {BirchItemType::kTab, BirchItemType::kSelfShare}, bar_chips));

  auto* root_window_controller = RootWindowController::ForWindow(root_window);
  // Right clicking on the wallpaper of the first display to show the context
  // menu.
  RightClickOn(root_window_controller->wallpaper_widget_controller()
                   ->GetWidget()
                   ->GetContentsView());

  auto* model_adapter =
      root_window_controller->menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  auto* sub_menu = model_adapter->root_for_testing()->GetSubmenu();
  auto* reset_item = sub_menu->GetMenuItemAt(7);
  EXPECT_EQ(reset_item->GetCommand(),
            base::to_underlying(BirchBarContextMenuModel::CommandId::kReset));

  // Clicking on the reset button to enable all suggestions pref and all four
  // types of suggestion chips should be shown on the bar.
  LeftClickOn(reset_item);
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kBirchUseCalendar));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kBirchUseFileSuggest));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kBirchUseRecentTabs));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kBirchUseSelfShare));

  EXPECT_EQ(4u, bar_chips.size());
  EXPECT_TRUE(
      has_suggestion_types({BirchItemType::kCalendar, BirchItemType::kFile,
                            BirchItemType::kTab, BirchItemType::kSelfShare},
                           bar_chips));
}

// The bar shows a maximum of 4 suggestion chips. The above test verifies
// resetting the first 4 suggestion types; this test verifies the rest.
TEST_P(BirchBarMenuTest, ResetSuggestionsExtended) {
  SetLastActiveItems(/*num=*/1);
  SetMostVisitedItems(/*num=*/1);

  // Enter Overview and check a bar view is created.
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);
  const auto& bar_chips = grid_test_api.GetBirchChips();

  // Disable the last active suggestions such that only most visited suggestions
  // are shown.
  auto* pref_service = GetPrefService();
  pref_service->SetBoolean(prefs::kBirchUseLastActive, false);

  EXPECT_EQ(1u, bar_chips.size());
  EXPECT_TRUE(HasSuggestionTypes({BirchItemType::kMostVisited}, bar_chips));

  auto* root_window_controller = RootWindowController::ForWindow(root_window);
  // Right clicking on the wallpaper of the first display to show the context
  // menu.
  RightClickOn(root_window_controller->wallpaper_widget_controller()
                   ->GetWidget()
                   ->GetContentsView());

  auto* model_adapter =
      root_window_controller->menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  auto* sub_menu = model_adapter->root_for_testing()->GetSubmenu();
  auto* reset_item = sub_menu->GetMenuItemAt(7);
  EXPECT_EQ(reset_item->GetCommand(),
            base::to_underlying(BirchBarContextMenuModel::CommandId::kReset));

  // Clicking on the reset button to enable all suggestions pref and all types
  // of suggestion chips should be shown on the bar.
  LeftClickOn(reset_item);
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kBirchUseLastActive));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kBirchUseMostVisited));

  EXPECT_EQ(2u, bar_chips.size());
  EXPECT_TRUE(HasSuggestionTypes(
      {BirchItemType::kLastActive, BirchItemType::kMostVisited}, bar_chips));
}

TEST_P(BirchBarMenuTest, ToggleFahrenheitCelsiusPref) {
  // The pref defaults to Fahrenheit.
  EXPECT_FALSE(GetPrefService()->GetBoolean(prefs::kBirchUseCelsius));

  // Show the birch bar with a weather item.
  SetWeatherItems(/*num=*/1);
  EnterOverview();

  // Get the overview grid test api.
  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);
  EXPECT_TRUE(grid_test_api.birch_bar_view());

  // Get the weather chip.
  raw_ptr<BirchChipButtonBase> chip = grid_test_api.GetBirchChips()[0];
  ASSERT_TRUE(chip);

  // Right-click on the chip to show the context menu.
  RightClickOn(chip);
  chip = nullptr;  // Avoid dangling pointer later.
  auto* model_adapter =
      BirchBarController::Get()->chip_menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  // Click on the menu item to toggle temperature units.
  auto* chip_menu = model_adapter->root_for_testing()->GetSubmenu();
  auto* toggle_temperature_units = chip_menu->GetMenuItemAt(2);
  EXPECT_EQ(toggle_temperature_units->GetCommand(),
            base::to_underlying(
                BirchChipContextMenuModel::CommandId::kToggleTemperatureUnits));
  LeftClickOn(toggle_temperature_units);

  // The pref is now set to use celsius.
  EXPECT_TRUE(GetPrefService()->GetBoolean(prefs::kBirchUseCelsius));
}

// Tests that there is no crash if hiding the suggestions by toggle the switch
// button in chip's submenu.
TEST_P(BirchBarMenuTest, NoCrashHideSuggestionsByChipSubmenu) {
  // Set show suggestions initially.
  GetPrefService()->SetBoolean(prefs::kBirchShowSuggestions, true);

  // Create a chip.
  SetCalendarItems(/*num=*/1);

  // Enter Overview and check a bar view is created.
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);

  // The birch bars should be shown.
  EXPECT_TRUE(grid_test_api.birch_bar_view());

  // Right clicking on a chip to show the chip menu.
  RightClickOn(grid_test_api.GetBirchChips()[0]);
  auto* model_adapter =
      BirchBarController::Get()->chip_menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  auto* chip_menu = model_adapter->root_for_testing()->GetSubmenu();
  auto* customize_suggestions_item = chip_menu->GetMenuItemAt(2);
  LeftClickOn(customize_suggestions_item);

  auto* sub_menu = customize_suggestions_item->GetSubmenu();
  auto* sub_show_suggestions_item = sub_menu->GetMenuItemAt(0);
  EXPECT_EQ(sub_show_suggestions_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kShowSuggestions));
  auto* switch_button =
      AsViewClass<Switch>(sub_show_suggestions_item->children()[0]);
  LeftClickOn(switch_button);
  EXPECT_FALSE(grid_test_api.birch_bar_view());
}

// Tests that there is no crash if customizing the suggestions by selecting the
// checkboxes in chip's submenu.
TEST_P(BirchBarMenuTest, NoCrashCustomizeSuggestionsByChipSubmenu) {
  // Set show suggestions and enable weather suggestions initially.
  GetPrefService()->SetBoolean(prefs::kBirchShowSuggestions, true);

  // Create 3 suggestions, one for each customizable suggestion type.
  SetCalendarItems(/*num=*/1);
  SetFileItems(/*num=*/1);
  SetTabItems(/*num=*/1);

  // Enter Overview and check a bar view is created.
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);

  // The birch bars should be shown.
  EXPECT_TRUE(grid_test_api.birch_bar_view());

  // Right clicking on a chip to show the chip menu.
  RightClickOn(grid_test_api.GetBirchChips()[0]);
  auto* model_adapter =
      BirchBarController::Get()->chip_menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  auto* chip_menu = model_adapter->root_for_testing()->GetSubmenu();
  auto* customize_suggestions_item = chip_menu->GetMenuItemAt(2);
  LeftClickOn(customize_suggestions_item);

  auto* sub_menu = customize_suggestions_item->GetSubmenu();
  auto* calendar_item = sub_menu->GetMenuItemAt(2);
  EXPECT_EQ(calendar_item->GetCommand(),
            base::to_underlying(
                BirchBarContextMenuModel::CommandId::kCalendarSuggestions));
  auto* calendar_checkbox =
      views::AsViewClass<Checkbox>(calendar_item->children()[0]);

  // Deselect the calendar.
  LeftClickOn(calendar_checkbox);
  EXPECT_FALSE(GetPrefService()->GetBoolean(prefs::kBirchUseCalendar));

  // The menu will be closed after selection.
  EXPECT_FALSE(
      BirchBarController::Get()->chip_menu_model_adapter_for_testing());

  RightClickOn(grid_test_api.GetBirchChips()[0]);
  model_adapter =
      BirchBarController::Get()->chip_menu_model_adapter_for_testing();
  EXPECT_TRUE(model_adapter->IsShowingMenu());

  customize_suggestions_item =
      model_adapter->root_for_testing()->GetSubmenu()->GetMenuItemAt(2);
  LeftClickOn(customize_suggestions_item);

  calendar_item = customize_suggestions_item->GetSubmenu()->GetMenuItemAt(2);
  calendar_checkbox =
      views::AsViewClass<Checkbox>(calendar_item->children()[0]);

  // Select the calendar.
  LeftClickOn(calendar_checkbox);
  EXPECT_TRUE(GetPrefService()->GetBoolean(prefs::kBirchUseCalendar));
}

// Tests hiding certain types of suggestions from context menu.
TEST_P(BirchBarMenuTest, HideSuggestionTypes) {
  // Create 4 types of suggestions, one for each customizable suggestion type.
  SetWeatherItems(/*num=*/1);
  SetCalendarItems(/*num=*/2);
  SetFileItems(/*num=*/2);
  SetTabItems(/*num=*/2);

  // Set show suggestions initially.
  GetPrefService()->SetBoolean(prefs::kBirchShowSuggestions, true);

  // Enter Overview and check a bar view is created.
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto grid_test_api = OverviewGridTestApi(root_window);

  // The birch bars should be shown.
  ASSERT_TRUE(grid_test_api.birch_bar_view());

  // Cache the chips.
  const auto& bar_chips = grid_test_api.GetBirchChips();

  while (bar_chips.size()) {
    BirchChipButtonBase* chip = bar_chips[0];

    SCOPED_TRACE(testing::Message()
                 << "Hide type of suggestion: " << chip->GetItem()->ToString());

    // Right clicking on a chip to show the chip menu.
    RightClickOn(chip);
    auto* model_adapter =
        BirchBarController::Get()->chip_menu_model_adapter_for_testing();
    EXPECT_TRUE(model_adapter->IsShowingMenu());

    auto* chip_menu = model_adapter->root_for_testing()->GetSubmenu();

    const BirchItemType item_type = chip->GetItem()->GetType();
    int hide_suggestions_type_idx = 1;
    if (item_type == BirchItemType::kWeather) {
      hide_suggestions_type_idx = 0;
    }

    auto* hide_suggestions_item =
        chip_menu->GetMenuItemAt(hide_suggestions_type_idx);

    int hide_suggestions_item_id = -1;
    std::string pref_name;
    switch (item_type) {
      case BirchItemType::kWeather:
        hide_suggestions_item_id = base::to_underlying(
            BirchChipContextMenuModel::CommandId::kHideWeatherSuggestions);
        pref_name = prefs::kBirchUseWeather;
        break;
      case BirchItemType::kCalendar:
        hide_suggestions_item_id = base::to_underlying(
            BirchChipContextMenuModel::CommandId::kHideCalendarSuggestions);
        pref_name = prefs::kBirchUseCalendar;
        break;
      case BirchItemType::kFile:
        hide_suggestions_item_id = base::to_underlying(
            BirchChipContextMenuModel::CommandId::kHideDriveSuggestions);
        pref_name = prefs::kBirchUseFileSuggest;
        break;
      case BirchItemType::kTab:
        hide_suggestions_item_id = base::to_underlying(
            BirchChipContextMenuModel::CommandId::kHideOtherDeviceSuggestions);
        pref_name = prefs::kBirchUseRecentTabs;
        break;
      default:
        break;
    }

    EXPECT_EQ(hide_suggestions_item_id, hide_suggestions_item->GetCommand());
    EXPECT_TRUE(GetPrefService()->GetBoolean(pref_name));

    LeftClickOn(hide_suggestions_item);

    // Corresponding type of suggestions should be hidden from the bar.
    EXPECT_FALSE(HasSuggestionTypes({item_type}, bar_chips));
    // Corresponding type of user prefs should be disabled.
    EXPECT_FALSE(GetPrefService()->GetBoolean(pref_name));
  }
}

// The parameter structure for birch bar responsive layout tests.
struct LayoutTestParams {
  gfx::Size display_size;
  ShelfAlignment shelf_alignment;
  //  Expected birch bar bounds with 1 to 4 chips in landscape mode.
  std::vector<gfx::Rect> expected_landscape_bounds;
  // Expected birch bar bounds with 1 to 4 chips in portrait mode.
  std::vector<gfx::Rect> expected_portrait_bounds;
};

////////////////////////////////////////////////////////////////////////////////
// BirchBarLayoutTest:
// The test class of birch bar layout.
class BirchBarLayoutTest
    : public BirchBarTestBase,
      public testing::WithParamInterface<LayoutTestParams> {
 public:
  BirchBarLayoutTest() : BirchBarTestBase(/*use_weather_v2_provider=*/false) {}
  BirchBarLayoutTest(const BirchBarLayoutTest&) = delete;
  BirchBarLayoutTest& operator=(const BirchBarLayoutTest&) = delete;
  ~BirchBarLayoutTest() override = default;

  // BirchBarTest:
  void SetUp() override {
    BirchBarTestBase::SetUp();

    // Clear existing items.
    birch_client_->Reset();

    // Set display size and shelf alignment according to the parameter.
    const LayoutTestParams params = GetParam();
    UpdateDisplay(params.display_size.ToString());

    // Here, we simulate changing the shelf alignment from context menu which
    // will update the user's pref. Otherwise, it will exit the Overview and
    // reset shelf alignment when we rotate the display.
    const int64_t display_id =
        display::Screen::GetScreen()->GetPrimaryDisplay().id();
    scoped_internal_display_id_ =
        std::make_unique<display::test::ScopedSetInternalDisplayId>(
            Shell::Get()->display_manager(), display_id);
    SetShelfAlignmentPref(
        Shell::Get()->session_controller()->GetPrimaryUserPrefService(),
        display_id, params.shelf_alignment);
  }

 private:
  std::unique_ptr<display::test::ScopedSetInternalDisplayId>
      scoped_internal_display_id_;
};

const LayoutTestParams kLayoutTestParams[] = {
    // The narrow display whose shorter side can only hold up to 2 chips.
    {/*display_size=*/gfx::Size(1080, 640),
     ShelfAlignment::kBottom,
     /*expected_landscape_bounds=*/
     {gfx::Rect(416, 512, 248, 64), gfx::Rect(288, 512, 504, 64),
      gfx::Rect(160, 512, 760, 64), gfx::Rect(32, 512, 1016, 64)},
     /*expected_portrait_bounds=*/
     {gfx::Rect(196, 952, 248, 64), gfx::Rect(68, 952, 504, 64),
      gfx::Rect(68, 880, 504, 136), gfx::Rect(68, 880, 504, 136)}},

    // The narrow display with shelf left aligned.
    {/*display_size=*/gfx::Size(1080, 640),
     ShelfAlignment::kLeft,
     /*expected_landscape_bounds=*/
     {gfx::Rect(436, 560, 240, 64), gfx::Rect(312, 560, 488, 64),
      gfx::Rect(188, 560, 736, 64), gfx::Rect(64, 560, 984, 64)},
     /*expected_portrait_bounds=*/
     {gfx::Rect(216, 1000, 240, 64), gfx::Rect(92, 1000, 488, 64),
      gfx::Rect(92, 928, 488, 136), gfx::Rect(92, 928, 488, 136)}},

    // The nearly squared display whose shorter side can hold up to 3 chips.
    {/*display_size=*/gfx::Size(1200, 1000),
     ShelfAlignment::kBottom,
     /*expected_landscape_bounds=*/
     {gfx::Rect(461, 872, 278, 64), gfx::Rect(318, 872, 564, 64),
      gfx::Rect(175, 872, 850, 64), gfx::Rect(32, 872, 1136, 64)},
     /*expected_portrait_bounds=*/
     {gfx::Rect(361, 1072, 278, 64), gfx::Rect(218, 1072, 564, 64),
      gfx::Rect(75, 1072, 850, 64), gfx::Rect(218, 1000, 564, 136)}},

    // The nearly squared display with shelf right aligned.
    {/*display_size=*/gfx::Size(1200, 1000),
     ShelfAlignment::kRight,
     /*expected_landscape_bounds=*/
     {gfx::Rect(449, 920, 270, 64), gfx::Rect(310, 920, 548, 64),
      gfx::Rect(171, 920, 826, 64), gfx::Rect(32, 920, 1104, 64)},
     /*expected_portrait_bounds=*/
     {gfx::Rect(349, 1120, 270, 64), gfx::Rect(210, 1120, 548, 64),
      gfx::Rect(71, 1120, 826, 64), gfx::Rect(210, 1048, 548, 136)}},

    // The wide display with width > 1450, which always use the optimal chip
    // size (278, 64).
    {/*display_size=*/gfx::Size(1600, 800),
     ShelfAlignment::kBottom,
     /*expected_landscape_bounds=*/
     {gfx::Rect(661, 672, 278, 64), gfx::Rect(518, 672, 564, 64),
      gfx::Rect(375, 672, 850, 64), gfx::Rect(232, 672, 1136, 64)},
     /*expected_portrait_bounds=*/
     {gfx::Rect(261, 1472, 278, 64), gfx::Rect(118, 1472, 564, 64),
      gfx::Rect(118, 1400, 564, 136), gfx::Rect(118, 1400, 564, 136)}},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BirchBarLayoutTest,
    testing::ValuesIn(kLayoutTestParams),
    [](const testing::TestParamInfo<BirchBarLayoutTest::ParamType>& info) {
      std::string test_name = info.param.display_size.ToString();

      switch (info.param.shelf_alignment) {
        case ShelfAlignment::kLeft:
          test_name += "_ShelfLeft";
          break;
        case ShelfAlignment::kRight:
          test_name = "_ShelfRight";
          break;
        case ShelfAlignment::kBottom:
        case ShelfAlignment::kBottomLocked:
          test_name += "_ShelfBottom";
      }
      return test_name;
    });

// Tests the responsive layout of a birch bar when converting from landscape
// mode to portrait mode with different number of chips.
TEST_P(BirchBarLayoutTest, ResponsiveLayout) {
  EnterOverview();

  aura::Window* root = Shell::GetPrimaryRootWindow();
  BirchBarView* birch_bar_view = OverviewGridTestApi(root).birch_bar_view();
  ASSERT_TRUE(birch_bar_view);

  const LayoutTestParams& params = GetParam();
  const views::Widget* birch_bar_widget =
      OverviewGridTestApi(root).birch_bar_widget();

  // Add test chips to the bar in landscape mode.
  std::vector<std::unique_ptr<BirchItem>> items_;
  for (int i = 0; i < 4; i++) {
    std::optional<std::u16string> secondary_action;
    if (i % 2) {
      secondary_action = u"add-on";
    }
    auto item = std::make_unique<TestBirchItem>(u"title", u"subtitle",
                                                secondary_action);
    birch_bar_view->AddChip(item.get());
    items_.emplace_back(std::move(item));
    EXPECT_EQ(birch_bar_widget->GetWindowBoundsInScreen(),
              params.expected_landscape_bounds[i]);
  }

  // Convert to portrait mode.
  ScreenOrientationControllerTestApi screen_rotation_test_api(
      Shell::Get()->screen_orientation_controller());
  screen_rotation_test_api.SetDisplayRotation(
      display::Display::ROTATE_90, display::Display::RotationSource::ACTIVE);

  // Removing chips from the bar in portrait mode.
  for (int i = 4; i > 0; i--) {
    EXPECT_EQ(birch_bar_widget->GetWindowBoundsInScreen(),
              params.expected_portrait_bounds[i - 1]);
    birch_bar_view->RemoveChip(items_[i - 1].get());
  }
}

// TODO(http://b/325335020): Add tests for tab traversal.

}  // namespace ash
