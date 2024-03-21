// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_client.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

namespace {

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
  const char* GetItemType() const override { return "Test"; }
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

  explicit TestBirchDataProvider(DataFetchedCallback data_fetched_callback)
      : data_fetched_callback_(data_fetched_callback) {}
  TestBirchDataProvider(const TestBirchDataProvider&) = delete;
  TestBirchDataProvider& operator=(const TestBirchDataProvider&) = delete;
  ~TestBirchDataProvider() override = default;

  void set_items(const std::vector<T>& items) { items_ = items; }

  void ClearItems() { items_.clear(); }

  // BirchDataProvider:
  void RequestBirchDataFetch() override { data_fetched_callback_.Run(items_); }

 private:
  DataFetchedCallback data_fetched_callback_;
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
                                base::Unretained(this)));
    file_provider_ = std::make_unique<TestBirchDataProvider<BirchFileItem>>(
        base::BindRepeating(&BirchModel::SetFileSuggestItems,
                            base::Unretained(birch_model)));
    tab_provider_ = std::make_unique<TestBirchDataProvider<BirchTabItem>>(
        base::BindRepeating(&BirchModel::SetRecentTabItems,
                            base::Unretained(birch_model)));
    release_notes_provider_ =
        std::make_unique<TestBirchDataProvider<BirchReleaseNotesItem>>(
            base::BindRepeating(&BirchModel::SetReleaseNotesItems,
                                base::Unretained(birch_model)));
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

  void SetReleaseNotesItems(const std::vector<BirchReleaseNotesItem>& items) {
    release_notes_provider_->set_items(items);
  }

  // Clear all items.
  void Reset() {
    calendar_provider_->ClearItems();
    file_provider_->ClearItems();
    tab_provider_->ClearItems();
    release_notes_provider_->ClearItems();
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
  BirchDataProvider* GetReleaseNotesProvider() override {
    return release_notes_provider_.get();
  }
  void WaitForRefreshTokens(base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  base::FilePath GetRemovedItemsFilePath() override { return base::FilePath(); }

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
  std::unique_ptr<TestBirchDataProvider<BirchReleaseNotesItem>>
      release_notes_provider_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BirchBarTest:
// The test class of birch bar with Forest feature enabled by default.
class BirchBarTest : public AshTestBase {
 public:
  BirchBarTest() {
    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchWeather}, {});
  }

  BirchBarTest(const BirchBarTest&) = delete;
  BirchBarTest& operator=(const BirchBarTest&) = delete;
  ~BirchBarTest() override = default;

  void SetUp() override {
    switches::SetIgnoreForestSecretKeyForTest(true);
    AshTestBase::SetUp();
    auto* birch_model = Shell::Get()->birch_model();
    birch_client_ = std::make_unique<TestBirchClient>(birch_model);
    birch_model->SetClientAndInit(birch_client_.get());
    auto weather_provider =
        std::make_unique<TestBirchDataProvider<BirchWeatherItem>>(
            base::BindRepeating(&BirchModel::SetWeatherItems,
                                base::Unretained(birch_model)));
    weather_provider_ = weather_provider.get();
    birch_model->OverrideWeatherProviderForTest(std::move(weather_provider));

    // Prepare a file item for test.
    std::vector<BirchFileItem> file_items;
    file_items.emplace_back(base::FilePath("test path"), u"Suggestion",
                            base::Time(), "file_id_0");
    file_items.back().set_ranking(1.0f);
    birch_client_->SetFileSuggestItems(file_items);
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    weather_provider_ = nullptr;
    birch_client_.reset();
    AshTestBase::TearDown();
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 protected:
  std::unique_ptr<TestBirchClient> birch_client_;
  raw_ptr<TestBirchDataProvider<BirchWeatherItem>> weather_provider_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the birch bar will be shown in the normal Overview.
TEST_F(BirchBarTest, ShowBirchBar) {
  EnterOverview();
  EXPECT_TRUE(
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).birch_bar_view());
}

TEST_F(BirchBarTest, RecordsHistogramWhenChipsShown) {
  base::HistogramTester histograms;

  // Add an ongoing calendar event at the current time. This will create a
  // suggestion chip.
  std::vector<BirchCalendarItem> items;
  items.emplace_back(u"Event", base::Time::Now() - base::Minutes(30),
                     base::Time::Now() + base::Minutes(30), GURL(), GURL(),
                     std::string());
  birch_client_->SetCalendarItems(items);

  // Entering overview shows the birch bar and records an impression.
  EnterOverview();
  base::RunLoop().RunUntilIdle();  // Wait for data fetch callback.
  histograms.ExpectBucketCount("Ash.Birch.Bar.Impression", true, 1);
}

// Tests that the birch bar will be hidden in the partial Overview with a split
// screen.
TEST_F(BirchBarTest, HideBirchBarInPartialSplitScreen) {
  // Create two windows.
  auto window_1 = CreateAppWindow(gfx::Rect(100, 100));
  // Need another window to keep partial Overview when `window_1` is snapped in
  // Overview session.
  auto window_2 = CreateAppWindow(gfx::Rect(100, 200));

  EnterOverview();

  // The birch bar should be shown in the normal Overview.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(OverviewGridTestApi(root_window).birch_bar_view());

  // Snap window 1 to create a split screen and the birch bar should be hidden.
  SplitViewController::Get(root_window)
      ->SnapWindow(window_1.get(), SnapPosition::kPrimary);
  EXPECT_FALSE(OverviewGridTestApi(root_window).birch_bar_view());

  // Dismiss the split screen, the birch bar should be shown.
  window_1.reset();
  EXPECT_TRUE(OverviewGridTestApi(root_window).birch_bar_view());
}

// Tests that the birch bar will be hidden in the Tablet mode.
// TODO(http://b/325963519): change this to test showing birch bar in tablet
// mode when the spec is finalized.
TEST_F(BirchBarTest, HideBirchBarInTabletMode) {
  EnterOverview();

  // The birch bar should be shown in the normal Overview.
  aura::Window* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(OverviewGridTestApi(root).birch_bar_view());

  // Convert to Tablet mode, the birch bar should be hidden.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_FALSE(OverviewGridTestApi(root).birch_bar_view());

  // Convert to Clamshell mode, the birch bar should be shown.
  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_TRUE(OverviewGridTestApi(root).birch_bar_view());
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
    : public BirchBarTest,
      public testing::WithParamInterface<LayoutTestParams> {
 public:
  BirchBarLayoutTest() = default;
  BirchBarLayoutTest(const BirchBarLayoutTest&) = delete;
  BirchBarLayoutTest& operator=(const BirchBarLayoutTest&) = delete;
  ~BirchBarLayoutTest() override = default;

  // BirchBarTest:
  void SetUp() override {
    BirchBarTest::SetUp();

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

}  // namespace ash
