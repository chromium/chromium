// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_unlock_throughput_recorder.h"

#include <memory>
#include <string>

#include "ash/login/ui/login_test_base.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/app_constants/constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

constexpr char kAshLoginAnimationDurationTabletMode[] =
    "Ash.LoginAnimation.Duration.TabletMode";
constexpr char kAshLoginAnimationDurationClamshellMode[] =
    "Ash.LoginAnimation.Duration.ClamshellMode";
constexpr char kAshUnlockAnimationDurationTabletMode[] =
    "Ash.UnlockAnimation.Duration.TabletMode";
constexpr char kAshUnlockAnimationDurationClamshellMode[] =
    "Ash.UnlockAnimation.Duration.ClamshellMode";

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
};

class TestObserver final : public ui::CompositorAnimationObserver {
 public:
  explicit TestObserver(ui::Compositor* compositor) : compositor_(compositor) {
    compositor_->AddAnimationObserver(this);
  }
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override {
    ++count_;
    if (count_ < 3)
      compositor_->ScheduleFullRedraw();
    else
      compositor_->RemoveAnimationObserver(this);
  }

  void OnCompositingShuttingDown(ui::Compositor* compositor) override {}

 private:
  int count_ = 0;
  const base::raw_ptr<ui::Compositor> compositor_;
};

class BeginMainFrameWaiter : public ui::CompositorObserver {
 public:
  explicit BeginMainFrameWaiter(ui::Compositor* compositor)
      : compositor_(compositor) {
    compositor->AddObserver(this);
  }

  ~BeginMainFrameWaiter() override { compositor_->RemoveObserver(this); }

  // ui::CompositorObserver
  void OnDidBeginMainFrame(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor_, compositor);
    done_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void Wait() {
    if (done_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  const base::raw_ptr<ui::Compositor> compositor_;
  bool done_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class FirstNonAnimatedFrameStartedWaiter : public ui::CompositorObserver {
 public:
  explicit FirstNonAnimatedFrameStartedWaiter(ui::Compositor* compositor)
      : compositor_(compositor) {
    compositor->AddObserver(this);
  }

  ~FirstNonAnimatedFrameStartedWaiter() override {
    compositor_->RemoveObserver(this);
  }

  // ui::CompositorObserver
  void OnFirstNonAnimatedFrameStarted(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor_, compositor);
    done_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void Wait() {
    if (done_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  ui::Compositor* compositor_;
  bool done_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
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

class MetricsWaiter {
 public:
  MetricsWaiter(base::HistogramTester* histogram_tester,
                std::string metrics_name)
      : histogram_tester_(histogram_tester), metrics_name_(metrics_name) {}

  MetricsWaiter(const MetricsWaiter&) = delete;
  MetricsWaiter& operator=(const MetricsWaiter&) = delete;

  ~MetricsWaiter() = default;

  void Wait() {
    while (histogram_tester_->GetTotalSum(metrics_name_) == 0) {
      GiveItSomeTime(base::Milliseconds(16));
    }
  }

 private:
  base::raw_ptr<base::HistogramTester> histogram_tester_;
  const std::string metrics_name_;
};

}  // namespace

// Test fixture for the LoginUnlockThroughputRecorder class.
class LoginUnlockThroughputRecorderTestBase
    : public LoginTestBase,
      public testing::WithParamInterface<bool> {
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

  void LockScreenAndAnimate() {
    GetSessionControllerClient()->LockScreen();
    RunSimpleAnimation();
  }

  void UnlockScreenAndAnimate() {
    GetSessionControllerClient()->UnlockScreen();
    RunSimpleAnimation();
  }

  void AddScheduledRestoreBrowserWindows(const std::vector<int>& ids,
                                         bool is_lacros) {
    for (int n : ids) {
      throughput_recorder()->AddScheduledRestoreWindow(
          n, is_lacros ? app_constants::kLacrosAppId : "",
          LoginUnlockThroughputRecorder::kBrowser);
    }
  }

  void AddScheduledRestoreNonBrowserWindows(const std::vector<int>& ids) {
    for (int n : ids) {
      throughput_recorder()->AddScheduledRestoreWindow(
          n, base::StringPrintf("some_app%d", n),
          LoginUnlockThroughputRecorder::kBrowser);
    }
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
      throughput_recorder()->OnRestoredWindowPresented(n);
    }
  }

 protected:
  void RunSimpleAnimation() {
    ui::Compositor* compositor =
        Shell::GetPrimaryRootWindow()->GetHost()->compositor();
    TestObserver observer(compositor);
    BeginMainFrameWaiter(compositor).Wait();
    FirstNonAnimatedFrameStartedWaiter(compositor).Wait();
    ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);
  }

  void EnableTabletMode(bool enable) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
  }

  LoginUnlockThroughputRecorder* throughput_recorder() {
    return Shell::Get()->login_unlock_throughput_recorder();
  }

  bool IsThroughputRecorderBlocked() {
    return throughput_recorder()
        ->login_animation_throughput_reporter()
        ->IsBlocked();
  }

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

using LoginUnlockThroughputRecorderLoginAnimationTest =
    LoginUnlockThroughputRecorderTestBase;

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
      GetParam() ? kAshLoginAnimationDurationTabletMode
                 : kAshLoginAnimationDurationClamshellMode;

  LoginOwner();
  RunSimpleAnimation();
  GiveItSomeTime(base::Milliseconds(100));

  // Should not report login histogram until shelf is initialized.
  EXPECT_EQ(histogram_tester_.get()->GetTotalSum(metrics_name), 0);

  // In this test case we ignore the shelf initialization. Pretend that it
  // was done.
  throughput_recorder()->ResetScopedThroughputReporterBlockerForTesting();
  RunSimpleAnimation();

  MetricsWaiter(histogram_tester_.get(),
                GetParam() ? kAshLoginAnimationDurationTabletMode
                           : kAshLoginAnimationDurationClamshellMode)
      .Wait();
}

// Verifies that login animation metrics are reported correctly after shelf is
// initialized.
TEST_P(LoginUnlockThroughputRecorderLoginAnimationTest,
       ReportLoginWithShelfInitialization) {
  EnableTabletMode(GetParam());
  const std::string metrics_name =
      GetParam() ? kAshLoginAnimationDurationTabletMode
                 : kAshLoginAnimationDurationClamshellMode;

  LoginOwner();
  GiveItSomeTime(base::Milliseconds(100));

  // Should not report login histogram until shelf is initialized.
  EXPECT_EQ(histogram_tester_.get()->GetTotalSum(metrics_name), 0);

  TestShelfModel model;
  model.InitializeIconList({1, 2, 3, 4, 5, 6});

  // None of the expected shelf items have icons loaded.
  throughput_recorder()->InitShelfIconList(&model);

  RunSimpleAnimation();
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
  model.SetIconsLoadedFor({7, 8, 9});
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
  RunSimpleAnimation();
  MetricsWaiter(histogram_tester_.get(), metrics_name).Wait();
}

TEST_P(LoginUnlockThroughputRecorderLoginAnimationTest, ReportUnlock) {
  LoginOwner();

  EnableTabletMode(GetParam());

  LockScreenAndAnimate();
  UnlockScreenAndAnimate();

  MetricsWaiter(histogram_tester_.get(),
                GetParam() ? kAshUnlockAnimationDurationTabletMode
                           : kAshUnlockAnimationDurationClamshellMode)
      .Wait();
}

using LoginUnlockThroughputRecorderWindowRestoreTest =
    LoginUnlockThroughputRecorderTestBase;

// Boolean parameter controls lacros mode.
INSTANTIATE_TEST_SUITE_P(All,
                         LoginUnlockThroughputRecorderWindowRestoreTest,
                         testing::Bool());

// Verifies that window restore metrics are reported correctly.
TEST_P(LoginUnlockThroughputRecorderWindowRestoreTest,
       ReportWindowRestoreMetrics) {
  const bool is_lacros = GetParam();

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

  AddScheduledRestoreBrowserWindows({1, 2, 3, 4, 5, 6}, is_lacros);
  AddScheduledRestoreNonBrowserWindows({7, 8, 9, 10, 11, 12});
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
  EXPECT_TRUE(histogram_tester_.get()->GetTotalSum(
      "Ash.LoginSessionRestore.AllBrowserWindowsPresented"));
}

}  // namespace ash
