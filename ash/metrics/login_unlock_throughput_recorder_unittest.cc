// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_unlock_throughput_recorder.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shell.h"
#include "ash/test/animation_metrics_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/app_constants/constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

constexpr char kAshLoginAnimationDuration2TabletMode[] =
    "Ash.LoginAnimation.Duration2.TabletMode";
constexpr char kAshLoginAnimationDuration2ClamshellMode[] =
    "Ash.LoginAnimation.Duration2.ClamshellMode";
constexpr char kBootTimeLogin3[] = "BootTime.Login3";

// A test shelf item delegate that simulates an activated window when a shelf
// item is selected.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    std::move(callback).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
  }
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

class TestShelfModel : public ShelfModel {
 public:
  TestShelfModel() = default;
  TestShelfModel(const TestShelfModel&) = delete;
  TestShelfModel& operator=(const TestShelfModel&) = delete;

  ~TestShelfModel() = default;

  void InitializeIconList(const std::vector<int>& ids) {
    while (!items().empty())
      RemoveItemAt(0);

    for (int n : ids) {
      ShelfItem item;
      item.id = ShelfID(base::StringPrintf("item%d", n));
      item.type = TYPE_PINNED_APP;
      Add(item, std::make_unique<TestShelfItemDelegate>(item.id));
    }
  }

  void AddBrowserIcon(bool is_lacros) {
    ShelfItem item;
    item.id = ShelfID(is_lacros ? app_constants::kLacrosAppId
                                : app_constants::kChromeAppId);
    item.type = TYPE_PINNED_APP;
    Add(item, std::make_unique<TestShelfItemDelegate>(item.id));
  }

  void SetIconsLoadedFor(const std::vector<int>& ids) {
    for (int n : ids) {
      const ShelfID id(base::StringPrintf("item%d", n));
      int index = ItemIndexByID(id);
      // Expect item exists.
      ASSERT_GE(index, 0);

      ShelfItem item = items()[index];
      item.image = gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

      Set(index, item);
    }
  }

  void SetIconLoadedForBrowser(bool is_lacros) {
    const ShelfID id(is_lacros ? app_constants::kLacrosAppId
                               : app_constants::kChromeAppId);
    int index = ItemIndexByID(id);
    // Expect item exists.
    ASSERT_GE(index, 0);

    ShelfItem item = items()[index];
    item.image = gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

    Set(index, item);
  }
};

void GiveItSomeTime(base::TimeDelta delta) {
  // Due to the |frames_to_terminate_tracker|=3 constant in
  // FrameSequenceTracker::ReportSubmitFrame we need to continue generating
  // frames to receive feedback.
  base::RepeatingTimer begin_main_frame_scheduler(
      FROM_HERE, base::Milliseconds(16), base::BindRepeating([]() {
        auto* compositor =
            Shell::GetPrimaryRootWindow()->GetHost()->compositor();
        compositor->ScheduleFullRedraw();
      }));
  begin_main_frame_scheduler.Reset();

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  auto* compositor = Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  compositor->ScheduleFullRedraw();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

}  // namespace

// Test fixture for the LoginUnlockThroughputRecorder class.
class LoginUnlockThroughputRecorderTestBase : public LoginTestBase {
 public:
  LoginUnlockThroughputRecorderTestBase() = default;

  LoginUnlockThroughputRecorderTestBase(
      const LoginUnlockThroughputRecorderTestBase&) = delete;
  LoginUnlockThroughputRecorderTestBase& operator=(
      const LoginUnlockThroughputRecorderTestBase&) = delete;

  ~LoginUnlockThroughputRecorderTestBase() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void LoginOwner() {
    CreateUserSessions(1);
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_OWNER);
  }

  void AddScheduledRestoreWindows(
      const std::vector<int>& browser_ids,
      bool is_lacros,
      const std::vector<int>& non_browser_ids = {}) {
    std::vector<LoginUnlockThroughputRecorder::RestoreWindowID> window_ids;
    for (int n : browser_ids) {
      std::string app_name =
          is_lacros ? app_constants::kLacrosAppId : app_constants::kChromeAppId;
      window_ids.emplace_back(n, std::move(app_name));
    }
    for (int n : non_browser_ids) {
      window_ids.emplace_back(n, base::StringPrintf("some_app%d", n));
    }
    throughput_recorder()->FullSessionRestoreDataLoaded(
        std::move(window_ids), /*restore_automatically=*/true);
  }

  void RestoredWindowsCreated(const std::vector<int>& ids) {
    for (int n : ids) {
      throughput_recorder()->OnRestoredWindowCreated(n);
    }
  }

  void RestoredWindowsShown(const std::vector<int>& ids) {
    ui::Compositor* compositor =
        Shell::GetPrimaryRootWindow()->GetHost()->compositor();
    for (int n : ids) {
      throughput_recorder()->OnBeforeRestoredWindowShown(n, compositor);
    }
  }

  void RestoredWindowsPresented(const std::vector<int>& ids) {
    for (int n : ids) {
      throughput_recorder()->window_restore_tracker()->OnPresentedForTesting(n);
    }
  }

 protected:
  void SetupDisplay(bool has_display) {
    if (has_display) {
      // A single default display will be configured automatically.
      return;
    }
    const std::vector<display::ManagedDisplayInfo> empty;
    display_manager()->OnNativeDisplaysChanged(empty);
    EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  }

  void EnableTabletMode(bool enable) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
  }

  LoginUnlockThroughputRecorder* throughput_recorder() {
    return Shell::Get()->login_unlock_throughput_recorder();
  }

  bool IsThroughputRecorderBlocked() {
    return throughput_recorder()
        ->GetLoginAnimationThroughputReporterForTesting()
        ->IsBlocked();
  }

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

class LoginUnlockThroughputRecorderLoginAnimationTest
    : public LoginUnlockThroughputRecorderTestBase,
      public testing::WithParamInterface<bool> {};

// Boolean parameter controls tablet mode.
INSTANTIATE_TEST_SUITE_P(All,
                         LoginUnlockThroughputRecorderLoginAnimationTest,
                         testing::Bool());

// Verifies that login animation metrics are reported correctly ignoring shelf
// initialization.
TEST_P(LoginUnlockThroughputRecorderLoginAnimationTest,
       ReportLoginAnimationOnly) {
  EnableTabletMode(GetParam());
  const std::string metrics_name =
      GetParam() ? kAshLoginAnimationDuration2TabletMode
                 : kAshLoginAnimationDuration2ClamshellMode;

  LoginOwner();
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  // Should not report login histogram until shelf is initialized.
  EXPECT_EQ(histogram_tester_.get()->GetTotalSum(metrics_name), 0);

  // In this test case we ignore the shelf initialization. Pretend that it
  // was done.
  throughput_recorder()->ResetScopedThroughputReporterBlockerForTesting();
  test::RunSimpleAnimation();

  test::MetricsWaiter(histogram_tester_.get(),
                      GetParam() ? kAshLoginAnimationDuration2TabletMode
                                 : kAshLoginAnimationDuration2ClamshellMode)
      .Wait();
}

// Verifies that login animation metrics are reported correctly after shelf is
// initialized.
TEST_P(LoginUnlockThroughputRecorderLoginAnimationTest,
       ReportLoginWithShelfInitialization) {
  EnableTabletMode(GetParam());
  const std::string metrics_name =
      GetParam() ? kAshLoginAnimationDuration2TabletMode
                 : kAshLoginAnimationDuration2ClamshellMode;

  LoginOwner();
  GiveItSomeTime(base::Milliseconds(100));

  // Do not expect any windows to be restored.
  throughput_recorder()->FullSessionRestoreDataLoaded(
      {}, /*restore_automatically=*/true);

  // Should not report login histogram until shelf is initialized.
  EXPECT_EQ(histogram_tester_.get()->GetTotalSum(metrics_name), 0);

  TestShelfModel model;
  model.InitializeIconList({1, 2, 3, 4, 5, 6});

  // None of the expected shelf items have icons loaded.
  throughput_recorder()->InitShelfIconList(&model);

  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  model.SetIconsLoadedFor({1, 2, 3});
  throughput_recorder()->UpdateShelfIconList(&model);
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  // Remove last shelf button.
  model.InitializeIconList({1, 2, 3, 4, 5});
  model.SetIconsLoadedFor({1, 2, 3});
  throughput_recorder()->UpdateShelfIconList(&model);
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  // Add extra buttons.
  model.InitializeIconList({4, 5, 6, 7, 8, 9});
  model.SetIconsLoadedFor({6, 7, 8, 9});
  // Only 4 and 5 are not loaded yet.
  throughput_recorder()->UpdateShelfIconList(&model);
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  model.SetIconsLoadedFor({4, 5});
  // All buttons should have icons.
  throughput_recorder()->UpdateShelfIconList(&model);
  // All loaded icons should trigger login histograms.
  EXPECT_FALSE(IsThroughputRecorderBlocked());
  EXPECT_GT(histogram_tester_.get()->GetTotalSum(
                "Ash.LoginSessionRestore.AllShelfIconsLoaded"),
            0);

  GiveItSomeTime(base::Milliseconds(100));
  // Should not report login histogram until login animation starts.
  EXPECT_EQ(histogram_tester_.get()->GetTotalSum(metrics_name), 0);
  // Shelf metrics should be already reported. Did not specifically start shelf
  // animations, but it should be reported immediately when there are no shelf
  // animation.
  EXPECT_GT(histogram_tester_.get()->GetTotalSum(
                "Ash.LoginSessionRestore.ShelfLoginAnimationEnd"),
            0);

  // Start login animation. It should trigger metrics reporting.
  test::RunSimpleAnimation();
  test::MetricsWaiter(histogram_tester_.get(), metrics_name).Wait();
}

class LoginUnlockThroughputRecorderWindowRestoreTest
    : public LoginUnlockThroughputRecorderTestBase,
      public testing::WithParamInterface<
          std::tuple</*is_lacros=*/bool, /*has_display=*/bool>> {};

INSTANTIATE_TEST_SUITE_P(All,
                         LoginUnlockThroughputRecorderWindowRestoreTest,
                         testing::Combine(/*is_lacros=*/testing::Bool(),
                                          /*has_display=*/testing::Bool()));

// Verifies that window restore metrics are reported correctly.
TEST_P(LoginUnlockThroughputRecorderWindowRestoreTest,
       ReportWindowRestoreMetrics) {
  const bool is_lacros = std::get<0>(GetParam());
  const bool has_display = std::get<1>(GetParam());
  SetupDisplay(has_display);

  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));

  LoginOwner();
  GiveItSomeTime(base::Milliseconds(100));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));

  AddScheduledRestoreWindows({1, 2, 3, 4, 5, 6}, is_lacros,
                             {7, 8, 9, 10, 11, 12});
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));

  // The unexpected windows do not trigger the metrics.
  RestoredWindowsCreated({21, 22, 23, 24, 25, 26});
  RestoredWindowsShown({21, 22, 23, 24, 25, 26});
  RestoredWindowsPresented({21, 22, 23, 24, 25, 26});
  GiveItSomeTime(base::Milliseconds(100));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));

  // Window must go through all of the expected steps
  // (Created->Shown->Presented). The non-created windows do not trigger
  // metrics.
  RestoredWindowsShown({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  RestoredWindowsPresented({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  GiveItSomeTime(base::Milliseconds(100));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));

  // Only wait for the expected browser windows: expected window 1 is missing.
  RestoredWindowsCreated({2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  RestoredWindowsShown({2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  RestoredWindowsPresented({2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  GiveItSomeTime(base::Milliseconds(100));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));

  // Last window created.
  RestoredWindowsCreated({1});
  EXPECT_TRUE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsCreated"));
  GiveItSomeTime(base::Milliseconds(100));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));
  EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));

  RestoredWindowsShown({1});
  EXPECT_TRUE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsShown"));

  RestoredWindowsPresented({1});
  if (has_display) {
    EXPECT_TRUE(histogram_tester_.get()->GetTotalSum(
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));
  } else {
    EXPECT_FALSE(histogram_tester_.get()->GetTotalSum(
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));
  }

  // Should not report login histograms until shelf icons are loaded.
  EXPECT_TRUE(histogram_tester_.get()
                  ->GetAllSamples(kAshLoginAnimationDuration2ClamshellMode)
                  .empty());
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.ShelfLoginAnimationEnd")
          .empty());
  EXPECT_TRUE(histogram_tester_.get()->GetAllSamples(kBootTimeLogin3).empty());
}

// Verifies that Login animation duration is reported when all shelf icons were
// loaded but only after windows were restored.
TEST_P(LoginUnlockThroughputRecorderWindowRestoreTest,
       ReportLoginAnimationDurationOnlyAfterWindowsRestore) {
  const bool is_lacros = std::get<0>(GetParam());
  const bool has_display = std::get<1>(GetParam());
  SetupDisplay(has_display);

  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.AllBrowserWindowsCreated")
          .empty());
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.AllBrowserWindowsShown")
          .empty());
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.AllBrowserWindowsPresented")
          .empty());

  LoginOwner();
  AddScheduledRestoreWindows({1, 2, 3}, is_lacros);
  // Should not report login histograms until shelf icons are loaded.
  EXPECT_TRUE(histogram_tester_.get()
                  ->GetAllSamples(kAshLoginAnimationDuration2ClamshellMode)
                  .empty());
  EXPECT_TRUE(histogram_tester_.get()->GetAllSamples(kBootTimeLogin3).empty());
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.ShelfLoginAnimationEnd")
          .empty());
  RestoredWindowsCreated({1, 2, 3});
  RestoredWindowsShown({1, 2, 3});
  RestoredWindowsPresented({1, 2, 3});

  test::MetricsWaiter(histogram_tester_.get(),
                      "Ash.LoginSessionRestore.AllBrowserWindowsCreated")
      .Wait();
  test::MetricsWaiter(histogram_tester_.get(),
                      "Ash.LoginSessionRestore.AllBrowserWindowsShown")
      .Wait();
  if (has_display) {
    test::MetricsWaiter(histogram_tester_.get(),
                        "Ash.LoginSessionRestore.AllBrowserWindowsPresented")
        .Wait();
  } else {
    EXPECT_TRUE(histogram_tester_.get()
                    ->GetAllSamples(
                        "Ash.LoginSessionRestore.AllBrowserWindowsPresented")
                    .empty());
  }

  TestShelfModel model;
  model.InitializeIconList({1, 2, 3});
  model.AddBrowserIcon(is_lacros);
  model.SetIconsLoadedFor({1, 2, 3});
  model.SetIconLoadedForBrowser(is_lacros);
  throughput_recorder()->InitShelfIconList(&model);

  // Start login animation. It should trigger metrics reporting.
  test::RunSimpleAnimation();
  test::MetricsWaiter(histogram_tester_.get(),
                      "Ash.LoginSessionRestore.ShelfLoginAnimationEnd")
      .Wait();
  test::MetricsWaiter(histogram_tester_.get(),
                      kAshLoginAnimationDuration2ClamshellMode)
      .Wait();
  test::MetricsWaiter(histogram_tester_.get(), kBootTimeLogin3).Wait();
}

// Verifies that Login animation duration is reported when all browser windows
// were restored but only after shelf icons were loaded.
TEST_P(LoginUnlockThroughputRecorderWindowRestoreTest,
       ReportLoginAnimationDurationOnlyAfterShelfIconsLoaded) {
  const bool is_lacros = std::get<0>(GetParam());
  const bool has_display = std::get<1>(GetParam());
  SetupDisplay(has_display);

  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.AllBrowserWindowsCreated")
          .empty());
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.AllBrowserWindowsShown")
          .empty());
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.AllBrowserWindowsPresented")
          .empty());
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.ShelfLoginAnimationEnd")
          .empty());
  EXPECT_TRUE(histogram_tester_.get()
                  ->GetAllSamples(kAshLoginAnimationDuration2ClamshellMode)
                  .empty());
  EXPECT_TRUE(histogram_tester_.get()->GetAllSamples(kBootTimeLogin3).empty());

  LoginOwner();

  TestShelfModel model;
  model.InitializeIconList({1, 2, 3});
  model.AddBrowserIcon(is_lacros);
  model.SetIconsLoadedFor({1, 2, 3});
  model.SetIconLoadedForBrowser(is_lacros);
  throughput_recorder()->InitShelfIconList(&model);
  test::RunSimpleAnimation();

  // Login is not completed until windows were restored.
  EXPECT_TRUE(
      histogram_tester_.get()
          ->GetAllSamples("Ash.LoginSessionRestore.ShelfLoginAnimationEnd")
          .empty());
  EXPECT_TRUE(histogram_tester_.get()
                  ->GetAllSamples(kAshLoginAnimationDuration2ClamshellMode)
                  .empty());
  EXPECT_TRUE(histogram_tester_.get()->GetAllSamples(kBootTimeLogin3).empty());
  GiveItSomeTime(base::Milliseconds(100));

  AddScheduledRestoreWindows({1, 2, 3}, is_lacros);
  RestoredWindowsCreated({1, 2, 3});
  RestoredWindowsShown({1, 2, 3});
  RestoredWindowsPresented({1, 2, 3});

  // Start login animation. It should trigger LoginAnimation.Duration reporting.
  test::RunSimpleAnimation();
  test::MetricsWaiter(histogram_tester_.get(),
                      "Ash.LoginSessionRestore.AllBrowserWindowsCreated")
      .Wait();
  test::MetricsWaiter(histogram_tester_.get(),
                      "Ash.LoginSessionRestore.AllBrowserWindowsShown")
      .Wait();
  if (has_display) {
    test::MetricsWaiter(histogram_tester_.get(),
                        "Ash.LoginSessionRestore.AllBrowserWindowsPresented")
        .Wait();
  } else {
    EXPECT_TRUE(histogram_tester_.get()
                    ->GetAllSamples(
                        "Ash.LoginSessionRestore.AllBrowserWindowsPresented")
                    .empty());
  }

  // Login metrics should be reported.
  // Start login animation. It should trigger LoginAnimation.Duration reporting.
  test::RunSimpleAnimation();
  test::MetricsWaiter(histogram_tester_.get(),
                      "Ash.LoginSessionRestore.ShelfLoginAnimationEnd")
      .Wait();
  test::MetricsWaiter(histogram_tester_.get(),
                      kAshLoginAnimationDuration2ClamshellMode)
      .Wait();
  test::MetricsWaiter(histogram_tester_.get(), kBootTimeLogin3).Wait();
}

}  // namespace ash
