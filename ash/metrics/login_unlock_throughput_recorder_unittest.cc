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
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::_;
using ::testing::MockFunction;
using ::testing::StrictMock;

namespace ash {
namespace {

class MockPostLoginEventObserver : public PostLoginEventObserver {
 public:
  explicit MockPostLoginEventObserver(
      LoginUnlockThroughputRecorder* login_unlock_throughput_recorder) {
    scoped_observation_.Observe(login_unlock_throughput_recorder);
  }

  MOCK_METHOD(void, OnAuthSuccess, (base::TimeTicks ts), (override));
  MOCK_METHOD(void,
              OnUserLoggedIn,
              (base::TimeTicks ts,
               bool is_ash_restarted,
               bool is_regular_user_or_owner),
              (override));
  MOCK_METHOD(void,
              OnAllExpectedShelfIconLoaded,
              (base::TimeTicks ts),
              (override));
  MOCK_METHOD(void,
              OnSessionRestoreDataLoaded,
              (base::TimeTicks ts, bool restore_automatically),
              (override));
  MOCK_METHOD(void,
              OnAllBrowserWindowsCreated,
              (base::TimeTicks ts),
              (override));
  MOCK_METHOD(void, OnAllBrowserWindowsShown, (base::TimeTicks ts), (override));
  MOCK_METHOD(void,
              OnAllBrowserWindowsPresented,
              (base::TimeTicks ts),
              (override));
  MOCK_METHOD(void, OnShelfAnimationFinished, (base::TimeTicks ts), (override));
  MOCK_METHOD(void,
              OnCompositorAnimationFinished,
              (base::TimeTicks ts,
               const cc::FrameSequenceMetrics::CustomReportData& data),
              (override));
  MOCK_METHOD(void, OnArcUiReady, (base::TimeTicks ts), (override));
  MOCK_METHOD(void,
              OnShelfIconsLoadedAndSessionRestoreDone,
              (base::TimeTicks ts),
              (override));
  MOCK_METHOD(void,
              OnShelfAnimationAndCompositorAnimationDone,
              (base::TimeTicks ts),
              (override));

 private:
  base::ScopedObservation<LoginUnlockThroughputRecorder, PostLoginEventObserver>
      scoped_observation_{this};
};

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
  }

  void LoginOwner() {
    CreateUserSessions(1);
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
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
  StrictMock<MockPostLoginEventObserver> mock_observer(throughput_recorder());
  StrictMock<MockFunction<void(const char* check_point_name)>> check_point;

  {
    ::testing::InSequence inseq;

    EXPECT_CALL(mock_observer,
                OnUserLoggedIn(_, /*is_ash_restarted=*/false,
                               /*is_regular_user_or_owner=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("login_done")).Times(1);

    EXPECT_CALL(mock_observer, OnCompositorAnimationFinished(_, _)).Times(1);

    EXPECT_CALL(check_point, Call("animation_done")).Times(1);
  }

  EnableTabletMode(GetParam());

  LoginOwner();
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("login_done");

  // In this test case we ignore the shelf initialization and session restore.
  // Pretend that it was done. It should only trigger
  // OnCompositorAnimationFinished.
  throughput_recorder()->ResetScopedThroughputReporterBlockerForTesting();
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("animation_done");
}

// Verifies that login animation metrics are reported correctly after shelf is
// initialized.
TEST_P(LoginUnlockThroughputRecorderLoginAnimationTest,
       ReportLoginWithShelfInitialization) {
  StrictMock<MockPostLoginEventObserver> mock_observer(throughput_recorder());
  StrictMock<MockFunction<void(const char* check_point_name)>> check_point;

  {
    ::testing::InSequence inseq;

    EXPECT_CALL(mock_observer,
                OnUserLoggedIn(_, /*is_ash_restarted=*/false,
                               /*is_regular_user_or_owner=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("login_done")).Times(1);

    EXPECT_CALL(mock_observer,
                OnSessionRestoreDataLoaded(_, /*restore_automatically=*/true))
        .Times(1);
    // NOTE: No browser restore events here since the restore list is empty in
    // this scenario.

    EXPECT_CALL(check_point, Call("restore_data_loaded")).Times(1);

    EXPECT_CALL(check_point, Call("shelf_initialized")).Times(1);
    EXPECT_CALL(check_point, Call("shelf_updated_1")).Times(1);
    EXPECT_CALL(check_point, Call("shelf_updated_2")).Times(1);
    EXPECT_CALL(check_point, Call("shelf_updated_3")).Times(1);

    EXPECT_CALL(mock_observer, OnAllExpectedShelfIconLoaded(_)).Times(1);
    EXPECT_CALL(mock_observer, OnShelfIconsLoadedAndSessionRestoreDone(_))
        .Times(1);
    // OnShelfAnimationFinished is triggered immediately as no shelf animation
    // is ongoing at this point.
    EXPECT_CALL(mock_observer, OnShelfAnimationFinished(_)).Times(1);

    EXPECT_CALL(check_point, Call("shelf_icons_loaded")).Times(1);

    EXPECT_CALL(mock_observer, OnCompositorAnimationFinished(_, _)).Times(1);
    EXPECT_CALL(mock_observer, OnShelfAnimationAndCompositorAnimationDone(_))
        .Times(1);

    EXPECT_CALL(check_point, Call("animation_done")).Times(1);
  }

  EnableTabletMode(GetParam());

  LoginOwner();
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("login_done");

  // Do not expect any windows to be restored.
  throughput_recorder()->FullSessionRestoreDataLoaded(
      {}, /*restore_automatically=*/true);

  check_point.Call("restore_data_loaded");

  TestShelfModel model;
  model.InitializeIconList({1, 2, 3, 4, 5, 6});

  // None of the expected shelf items have icons loaded.
  throughput_recorder()->InitShelfIconList(&model);

  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  check_point.Call("shelf_initialized");

  model.SetIconsLoadedFor({1, 2, 3});
  throughput_recorder()->UpdateShelfIconList(&model);
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  check_point.Call("shelf_updated_1");

  // Remove last shelf button.
  model.InitializeIconList({1, 2, 3, 4, 5});
  model.SetIconsLoadedFor({1, 2, 3});
  throughput_recorder()->UpdateShelfIconList(&model);
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  check_point.Call("shelf_updated_2");

  // Add extra buttons.
  model.InitializeIconList({4, 5, 6, 7, 8, 9});
  model.SetIconsLoadedFor({6, 7, 8, 9});
  // Only 4 and 5 are not loaded yet.
  throughput_recorder()->UpdateShelfIconList(&model);
  EXPECT_TRUE(IsThroughputRecorderBlocked());

  check_point.Call("shelf_updated_3");

  model.SetIconsLoadedFor({4, 5});
  // All buttons should have icons.
  throughput_recorder()->UpdateShelfIconList(&model);
  // All loaded icons should trigger login histograms.
  EXPECT_FALSE(IsThroughputRecorderBlocked());

  check_point.Call("shelf_icons_loaded");

  // Start login animation.
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("animation_done");
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

  StrictMock<MockPostLoginEventObserver> mock_observer(throughput_recorder());
  StrictMock<MockFunction<void(const char* check_point_name)>> check_point;

  {
    ::testing::InSequence inseq;

    EXPECT_CALL(mock_observer,
                OnUserLoggedIn(_, /*is_ash_restarted=*/false,
                               /*is_regular_user_or_owner=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("login_done")).Times(1);

    EXPECT_CALL(mock_observer,
                OnSessionRestoreDataLoaded(_, /*restore_automatically=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("restore_windows_scheduled")).Times(1);
    EXPECT_CALL(check_point, Call("restore_ongoing_1")).Times(1);
    EXPECT_CALL(check_point, Call("restore_ongoing_2")).Times(1);
    EXPECT_CALL(check_point, Call("restore_ongoing_3")).Times(1);
    EXPECT_CALL(mock_observer, OnAllBrowserWindowsCreated(_)).Times(1);
    EXPECT_CALL(check_point, Call("last_window_created")).Times(1);

    if (has_display) {
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsShown(_)).Times(1);
      EXPECT_CALL(check_point, Call("last_window_shown")).Times(1);
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsPresented(_)).Times(1);
      EXPECT_CALL(check_point, Call("last_window_presented")).Times(1);
    } else {
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsShown(_)).Times(1);
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsPresented(_)).Times(1);
      EXPECT_CALL(check_point, Call("last_window_shown")).Times(1);
      EXPECT_CALL(check_point, Call("last_window_presented")).Times(1);
    }
  }

  SetupDisplay(has_display);

  LoginOwner();
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("login_done");

  AddScheduledRestoreWindows({1, 2, 3, 4, 5, 6}, is_lacros,
                             {7, 8, 9, 10, 11, 12});

  check_point.Call("restore_windows_scheduled");

  // The unexpected windows do not trigger the metrics.
  RestoredWindowsCreated({21, 22, 23, 24, 25, 26});
  RestoredWindowsShown({21, 22, 23, 24, 25, 26});
  RestoredWindowsPresented({21, 22, 23, 24, 25, 26});

  check_point.Call("restore_ongoing_1");

  // Window must go through all of the expected steps
  // (Created->Shown->Presented). The non-created windows do not trigger
  // metrics.
  RestoredWindowsShown({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  RestoredWindowsPresented({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});

  check_point.Call("restore_ongoing_2");

  // Only wait for the expected browser windows: expected window 1 is missing.
  RestoredWindowsCreated({2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  RestoredWindowsShown({2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  RestoredWindowsPresented({2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});

  check_point.Call("restore_ongoing_3");

  // Last window created.
  RestoredWindowsCreated({1});

  check_point.Call("last_window_created");

  RestoredWindowsShown({1});

  check_point.Call("last_window_shown");

  RestoredWindowsPresented({1});

  check_point.Call("last_window_presented");
}

// Verifies that Login animation duration is reported when all shelf icons were
// loaded but only after windows were restored.
TEST_P(LoginUnlockThroughputRecorderWindowRestoreTest,
       ReportLoginAnimationDurationOnlyAfterWindowsRestore) {
  const bool is_lacros = std::get<0>(GetParam());
  const bool has_display = std::get<1>(GetParam());

  StrictMock<MockPostLoginEventObserver> mock_observer(throughput_recorder());
  StrictMock<MockFunction<void(const char* check_point_name)>> check_point;

  {
    ::testing::InSequence inseq;

    EXPECT_CALL(mock_observer,
                OnUserLoggedIn(_, /*is_ash_restarted=*/false,
                               /*is_regular_user_or_owner=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("login_done")).Times(1);

    EXPECT_CALL(mock_observer,
                OnSessionRestoreDataLoaded(_, /*restore_automatically=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("restore_windows_scheduled")).Times(1);

    EXPECT_CALL(mock_observer, OnAllBrowserWindowsCreated(_)).Times(1);

    EXPECT_CALL(check_point, Call("all_windows_created")).Times(1);

    if (has_display) {
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsShown(_)).Times(1);
      EXPECT_CALL(check_point, Call("all_windows_shown")).Times(1);
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsPresented(_)).Times(1);
      EXPECT_CALL(check_point, Call("all_windows_presented")).Times(1);
    } else {
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsShown(_)).Times(1);
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsPresented(_)).Times(1);
      EXPECT_CALL(check_point, Call("all_windows_shown")).Times(1);
      EXPECT_CALL(check_point, Call("all_windows_presented")).Times(1);
    }

    EXPECT_CALL(mock_observer, OnAllExpectedShelfIconLoaded(_)).Times(1);
    EXPECT_CALL(mock_observer, OnShelfIconsLoadedAndSessionRestoreDone(_))
        .Times(1);
    // OnShelfAnimationFinished is triggered immediately as no shelf animation
    // is ongoing at this point.
    EXPECT_CALL(mock_observer, OnShelfAnimationFinished(_)).Times(1);

    EXPECT_CALL(check_point, Call("shelf_icons_loaded")).Times(1);

    EXPECT_CALL(mock_observer, OnCompositorAnimationFinished(_, _)).Times(1);
    EXPECT_CALL(mock_observer, OnShelfAnimationAndCompositorAnimationDone(_))
        .Times(1);

    EXPECT_CALL(check_point, Call("animation_done")).Times(1);
  }

  SetupDisplay(has_display);

  LoginOwner();
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("login_done");

  AddScheduledRestoreWindows({1, 2, 3}, is_lacros);

  check_point.Call("restore_windows_scheduled");

  RestoredWindowsCreated({1, 2, 3});

  check_point.Call("all_windows_created");

  RestoredWindowsShown({1, 2, 3});

  check_point.Call("all_windows_shown");

  RestoredWindowsPresented({1, 2, 3});

  check_point.Call("all_windows_presented");

  TestShelfModel model;
  model.InitializeIconList({1, 2, 3});
  model.AddBrowserIcon(is_lacros);
  model.SetIconsLoadedFor({1, 2, 3});
  model.SetIconLoadedForBrowser(is_lacros);
  throughput_recorder()->InitShelfIconList(&model);

  check_point.Call("shelf_icons_loaded");

  // Start login animation.
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("animation_done");
}

// Verifies that Login animation duration is reported when all browser windows
// were restored but only after shelf icons were loaded.
TEST_P(LoginUnlockThroughputRecorderWindowRestoreTest,
       ReportLoginAnimationDurationOnlyAfterShelfIconsLoaded) {
  const bool is_lacros = std::get<0>(GetParam());
  const bool has_display = std::get<1>(GetParam());

  StrictMock<MockPostLoginEventObserver> mock_observer(throughput_recorder());
  StrictMock<MockFunction<void(const char* check_point_name)>> check_point;

  {
    ::testing::InSequence inseq;

    EXPECT_CALL(mock_observer,
                OnUserLoggedIn(_, /*is_ash_restarted=*/false,
                               /*is_regular_user_or_owner=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("login_done")).Times(1);

    EXPECT_CALL(mock_observer, OnAllExpectedShelfIconLoaded(_)).Times(1);

    EXPECT_CALL(check_point, Call("shelf_icons_loaded")).Times(1);

    EXPECT_CALL(mock_observer,
                OnSessionRestoreDataLoaded(_, /*restore_automatically=*/true))
        .Times(1);

    EXPECT_CALL(check_point, Call("restore_windows_scheduled")).Times(1);

    EXPECT_CALL(mock_observer, OnAllBrowserWindowsCreated(_)).Times(1);

    EXPECT_CALL(check_point, Call("all_windows_created")).Times(1);

    if (has_display) {
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsShown(_)).Times(1);
      EXPECT_CALL(check_point, Call("all_windows_shown")).Times(1);
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsPresented(_)).Times(1);
      EXPECT_CALL(mock_observer, OnShelfIconsLoadedAndSessionRestoreDone(_))
          .Times(1);
      EXPECT_CALL(mock_observer, OnShelfAnimationFinished(_)).Times(1);

      EXPECT_CALL(check_point, Call("all_windows_presented")).Times(1);
    } else {
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsShown(_)).Times(1);
      EXPECT_CALL(mock_observer, OnAllBrowserWindowsPresented(_)).Times(1);
      EXPECT_CALL(mock_observer, OnShelfIconsLoadedAndSessionRestoreDone(_))
          .Times(1);
      EXPECT_CALL(mock_observer, OnShelfAnimationFinished(_)).Times(1);

      EXPECT_CALL(check_point, Call("all_windows_shown")).Times(1);
      EXPECT_CALL(check_point, Call("all_windows_presented")).Times(1);
    }

    EXPECT_CALL(mock_observer, OnCompositorAnimationFinished(_, _)).Times(1);
    EXPECT_CALL(mock_observer, OnShelfAnimationAndCompositorAnimationDone(_))
        .Times(1);

    EXPECT_CALL(check_point, Call("animation_done")).Times(1);
  }

  SetupDisplay(has_display);

  LoginOwner();
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("login_done");

  TestShelfModel model;
  model.InitializeIconList({1, 2, 3});
  model.AddBrowserIcon(is_lacros);
  model.SetIconsLoadedFor({1, 2, 3});
  model.SetIconLoadedForBrowser(is_lacros);
  throughput_recorder()->InitShelfIconList(&model);
  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("shelf_icons_loaded");

  AddScheduledRestoreWindows({1, 2, 3}, is_lacros);

  check_point.Call("restore_windows_scheduled");

  RestoredWindowsCreated({1, 2, 3});

  check_point.Call("all_windows_created");

  RestoredWindowsShown({1, 2, 3});

  check_point.Call("all_windows_shown");

  RestoredWindowsPresented({1, 2, 3});

  check_point.Call("all_windows_presented");

  test::RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  check_point.Call("animation_done");
}

}  // namespace ash
