// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/overview_tracing_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/test/arc_task_window_builder.h"
#include "ash/constants/ash_switches.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/ash/arc/tracing/test/overview_tracing_test_handler.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace arc {

using EventType = arc::ArcTracingGraphicsModel::EventType;

namespace {

constexpr std::string_view kBasicSystrace =
    "{\"traceEvents\":[],\"systemTraceEvents\":\""
    // clang-format off
    "          <idle>-0     [003] d..0 44442.000001: cpu_idle: state=0 cpu_id=3\n"
    // clang-format on
    "\"}";

class OverviewTracingHandlerTest : public ChromeAshTestBase {
 public:
  OverviewTracingHandlerTest()
      : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))),
        weak_ptr_factory_(this) {}

  ~OverviewTracingHandlerTest() override = default;

  OverviewTracingHandlerTest(const OverviewTracingHandlerTest&) = delete;
  OverviewTracingHandlerTest& operator=(const OverviewTracingHandlerTest&) =
      delete;

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.SetUp(profile_.get());

    // WMHelper constructor sets a global instance which the Handler constructor
    // requires.
    wm_helper_ = std::make_unique<exo::WMHelper>();
    download_path_ = base::GetTempDirForTesting();
    handler_ = std::make_unique<OverviewTracingTestHandler>(
        base::BindRepeating(&OverviewTracingHandlerTest::OnArcWindowFocusChange,
                            weak_ptr_factory_.GetWeakPtr()));
    handler_->set_start_build_model_cb(
        base::BindRepeating(&OverviewTracingHandlerTest::OnStartBuildModel,
                            weak_ptr_factory_.GetWeakPtr()));
    handler_->set_graphics_model_ready_cb(
        base::BindRepeating(&OverviewTracingHandlerTest::OnGraphicsModelReady,
                            weak_ptr_factory_.GetWeakPtr()));

    local_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        local_pref_service_.get());
    arc::prefs::RegisterLocalStatePrefs(local_pref_service_->registry());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kEnableArcVm);

    saved_tz_.reset(icu::TimeZone::createDefault());
  }

  static void SetTimeZone(const char* name) {
    std::unique_ptr<icu::TimeZone> tz{icu::TimeZone::createTimeZone(name)};
    icu::TimeZone::setDefault(*tz);
  }

  void TearDown() override {
    icu::TimeZone::setDefault(*saved_tz_);

    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    local_pref_service_.reset();

    handler_.reset();
    wm_helper_.reset();

    arc_app_test_.TearDown();

    profile_.reset();

    ChromeAshTestBase::TearDown();
  }

 protected:
  void FastForwardClockAndTaskQueue(base::TimeDelta delta) {
    handler_->set_now(handler_->Now() + delta);
    task_environment()->FastForwardBy(delta);
  }

  void OnArcWindowFocusChange(aura::Window* window) {
    if (window) {
      events_.push_back("new ARC window focused");
    } else {
      events_.push_back("ARC window loses focus");
    }
  }

  void OnStartBuildModel() { events_.push_back("start building model"); }
  void OnGraphicsModelReady(std::unique_ptr<OverviewTracingResult> results) {
    events_.push_back(
        base::StrCat({"graphics model status: ", results->status}));
    model_ = std::move(results->model);
  }

  // The time relative to which trace tick timestamps are calculated. This is
  // typically when the system was booted.
  base::Time trace_time_base_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
  base::FilePath download_path_;
  std::unique_ptr<OverviewTracingTestHandler> handler_;
  std::unique_ptr<icu::TimeZone> saved_tz_;
  std::vector<std::string> events_;
  base::Value model_;

  std::unique_ptr<TestingPrefServiceSimple> local_pref_service_;

  base::WeakPtrFactory<OverviewTracingHandlerTest> weak_ptr_factory_;
};

TEST_F(OverviewTracingHandlerTest, ModelName) {
  base::FilePath download_path = base::FilePath::FromASCII("/mnt/downloads");
  base::Time timestamp = base::Time::UnixEpoch() + base::Seconds(1);

  std::unique_ptr<icu::TimeZone> tz;
  SetTimeZone("America/Chicago");
  EXPECT_EQ("overview_tracing_test_title_1_1969-12-31_18-00-01.json",
            handler_->GetModelBaseNameFromTitle("Test Title #:1", timestamp));
  SetTimeZone("Indian/Maldives");
  EXPECT_EQ(
      "overview_tracing_0123456789012345678901234567890_"
      "1970-01-01_05-00-01.json",
      handler_->GetModelBaseNameFromTitle(
          "0123456789012345678901234567890123456789", timestamp));

  SetTimeZone("Etc/UTC");
  timestamp = base::Time::UnixEpoch() + base::Days(50);
  EXPECT_EQ("overview_tracing_xyztitle_1970-02-20_00-00-00.json",
            handler_->GetModelBaseNameFromTitle("xyztitle", timestamp));

  SetTimeZone("Japan");
  EXPECT_EQ("overview_tracing_secret_app_1970-02-20_09-00-00.json",
            handler_->GetModelBaseNameFromTitle("Secret App", timestamp));
}

TEST_F(OverviewTracingHandlerTest, FilterSystemTraceByTimestamp) {
  handler_->set_now(
      base::Time::FromMillisecondsSinceUnixEpoch(1'500'088'880'000));
  handler_->set_trace_time_base(
      base::Time::FromMillisecondsSinceUnixEpoch(1'500'000'000'000));

  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetShellRootSurface(&s)
                        .BuildOwnsNativeWidget();

  arc_widget->Show();
  auto max_time = base::Seconds(5);
  handler_->StartTracing(download_path_, max_time);
  handler_->StartTracingOnControllerRespond();

  // Fast forward past the max tracing interval.
  FastForwardClockAndTaskQueue(max_time + base::Milliseconds(500));

  // Pass results from trace controller to handler. First and last events should
  // not be in the model.
  handler_->StopTracingOnControllerRespond(std::make_unique<std::string>(
      R"(
{
    "traceEvents": [],
    "systemTraceEvents":
"          <idle>-0     [000] d..0 88879.800000: sched_wakeup: comm=foo pid=99 prio=115 target_cpu=000
          <idle>-0     [000] d..0 88882.000001: cpu_idle: state=0 cpu_id=0
          <idle>-0     [000] dn.0 88883.000002: cpu_idle: state=4294967295 cpu_id=0
          <idle>-0     [000] dnh3 88884.000003: sched_wakeup: comm=foo pid=25821 prio=115 target_cpu=000
          <idle>-0     [000] d..3 88884.500004: sched_switch: prev_comm=bar prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=baz next_pid=25891 next_prio=115
          <idle>-0     [000] d..3 88885.500004: sched_switch: prev_comm=baz prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=foo next_pid=33921 next_prio=115
"
})"));

  ASSERT_EQ(("new ARC window focused\n"
             "start building model"),
            base::JoinString(events_, "\n"));

  task_environment()->RunUntilIdle();

  ASSERT_EQ(("new ARC window focused\n"
             "start building model\n"
             "graphics model status: Tracing model is ready"),
            base::JoinString(events_, "\n"));
  {
    const auto& dict = model_.GetDict();
    const auto* events_by_cpu = dict.FindListByDottedPath("system.cpu");
    ASSERT_TRUE(events_by_cpu);
    // Only one CPU in log.
    ASSERT_EQ(1UL, events_by_cpu->size());

    const auto& cpu_events = (*events_by_cpu)[0].GetList();
    ASSERT_EQ(4UL, cpu_events.size()) << cpu_events;

    EXPECT_EQ(25821.0, cpu_events[2].GetList()[2].GetDouble());
    EXPECT_EQ(25891.0, cpu_events[3].GetList()[2].GetDouble());
  }
}

TEST_F(OverviewTracingHandlerTest, SwitchWindowDuringModelBuild) {
  handler_->set_now(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'044'440'000));
  handler_->set_trace_time_base(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'000'000'000));

  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s)
                        .BuildOwnsNativeWidget();

  auto other_arc_widget = arc::ArcTaskWindowBuilder()
                              .SetTaskId(88)
                              .SetPackageName("net.differentapp")
                              .SetShellRootSurface(&s)
                              .BuildOwnsNativeWidget();

  arc_widget->Show();
  other_arc_widget->ShowInactive();
  auto max_time = base::Seconds(5);
  handler_->StartTracing(download_path_, max_time);
  handler_->StartTracingOnControllerRespond();

  // Fast forward past the max tracing interval. This will stop the trace at the
  // end of the fast-forward, which is 400ms after the timeout.
  FastForwardClockAndTaskQueue(max_time + base::Milliseconds(400));

  // While model is being built, switch to the ARC window to change
  // min_tracing_time_. This sets the min trace time to 300ms after the end of
  // the trace.
  FastForwardClockAndTaskQueue(base::Milliseconds(300));
  other_arc_widget->Activate();

  // Pass results from trace controller to handler.
  handler_->StopTracingOnControllerRespond(
      std::make_unique<std::string>(kBasicSystrace));

  task_environment()->RunUntilIdle();

  {
    const auto& dict = model_.GetDict();
    const auto* events_by_cpu = dict.FindListByDottedPath("system.cpu");
    ASSERT_TRUE(events_by_cpu);
    ASSERT_EQ(4UL, events_by_cpu->size());

    const auto& cpu_events = (*events_by_cpu)[3].GetList();
    ASSERT_EQ(1UL, cpu_events.size()) << cpu_events;
  }
}

TEST_F(OverviewTracingHandlerTest, SwitchWindowDuringTrace) {
  handler_->set_now(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'044'440'000));
  handler_->set_trace_time_base(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'000'000'000));

  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s)
                        .SetTitle("the first app")
                        .BuildOwnsNativeWidget();

  auto other_arc_widget = arc::ArcTaskWindowBuilder()
                              .SetTaskId(88)
                              .SetPackageName("net.differentapp")
                              .SetShellRootSurface(&s)
                              .SetTitle("another ARC app")
                              .BuildOwnsNativeWidget();

  arc_widget->Show();
  other_arc_widget->ShowInactive();
  auto max_time = base::Seconds(5);
  handler_->StartTracing(download_path_, max_time);
  handler_->StartTracingOnControllerRespond();

  FastForwardClockAndTaskQueue(base::Seconds(1));
  other_arc_widget->Activate();
  FastForwardClockAndTaskQueue(base::Seconds(1));
  task_environment()->RunUntilIdle();

  // Fast forward past the max tracing interval. This will stop the trace at the
  // end of the fast-forward, which is 400ms after the timeout.
  FastForwardClockAndTaskQueue(base::Milliseconds(4500));

  // Pass results from trace controller to handler.
  handler_->StopTracingOnControllerRespond(
      std::make_unique<std::string>(kBasicSystrace));

  task_environment()->RunUntilIdle();

  // The current behavior when activating a separate window during a trace is to
  // stop the trace and switch to the web UI. This behavior may change, but this
  // test was originally added to avoid a DCHECK failure upon switching the
  // active window. NOTE even if the below asserts are no longer valid, this
  // test is still important for what has occurred before this line.
  const auto& dict = model_.GetDict();
  EXPECT_EQ(*dict.FindStringByDottedPath("information.title"), "the first app");
}

TEST_F(OverviewTracingHandlerTest, SwitchWindowBeforeTraceStart) {
  handler_->set_now(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'044'440'000));
  handler_->set_trace_time_base(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'000'000'000));

  exo::Surface s1, s2;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s1)
                        .BuildOwnsNativeWidget();

  auto other_arc_widget = arc::ArcTaskWindowBuilder()
                              .SetTaskId(88)
                              .SetPackageName("net.differentapp")
                              .SetTitle("i will be traced")
                              .SetShellRootSurface(&s2)
                              .BuildOwnsNativeWidget();

  arc_widget->Show();
  other_arc_widget->ShowInactive();
  handler_->StartTracing(download_path_, base::Seconds(5));
  other_arc_widget->Activate();

  handler_->StartTracingOnControllerRespond();
  FastForwardClockAndTaskQueue(base::Seconds(6));
  handler_->VerifyNoUnrespondedCallback();

  // We should be able to do a trace now - no trace state should be lingering.
  // The web UI will be active - switch back to an ARC app.
  other_arc_widget->Activate();
  handler_->StartTracing(download_path_, base::Seconds(5));
  handler_->StartTracingOnControllerRespond();
  FastForwardClockAndTaskQueue(base::Seconds(6));
  handler_->StopTracingOnControllerRespond(
      std::make_unique<std::string>(kBasicSystrace));
  task_environment()->RunUntilIdle();

  const auto& dict = model_.GetDict();
  EXPECT_EQ(*dict.FindStringByDottedPath("information.title"),
            "i will be traced");
}

TEST_F(OverviewTracingHandlerTest, CommitAndPresentTimestampsInModel) {
  handler_->set_now(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'044'440'000));
  handler_->set_trace_time_base(
      base::Time::FromMillisecondsSinceUnixEpoch(1'600'000'000'000));

  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s)
                        .SetTitle("the first app")
                        .BuildOwnsNativeWidget();

  arc_widget->Show();
  handler_->StartTracing(download_path_, base::Seconds(5));
  handler_->StartTracingOnControllerRespond();

  FastForwardClockAndTaskQueue(base::Seconds(1));

  for (int i = 0; i < 5; i++) {
    s.Commit();
    std::list<exo::Surface::FrameCallback> frame_callbacks;
    std::list<exo::Surface::PresentationCallback> presentation_callbacks;
    s.AppendSurfaceHierarchyCallbacks(&frame_callbacks,
                                      &presentation_callbacks);
    gfx::PresentationFeedback feedback(handler_->SystemTicksNow(),
                                       base::TimeDelta() /* interval */,
                                       0 /* flags */);
    for (auto& cb : presentation_callbacks) {
      cb.Run(feedback);
    }
    FastForwardClockAndTaskQueue(base::Milliseconds(42));
  }

  FastForwardClockAndTaskQueue(base::Seconds(5));

  handler_->StopTracingOnControllerRespond(
      std::make_unique<std::string>(kBasicSystrace));

  task_environment()->RunUntilIdle();

  const auto& dict = model_.GetDict();

  LOG(INFO) << "checking JSON model: " << dict;

  auto validate_events = [&](EventType type, const base::Value::List* events) {
    ASSERT_TRUE(events);
    std::queue<double> expected_times({0, 42000, 84000, 126000, 168000});
    ASSERT_EQ(events->size(), expected_times.size());
    for (const auto& event : *events) {
      const auto& list = event.GetList();
      ASSERT_EQ(list.size(), 2ul);
      ASSERT_EQ(list[0].GetInt(), static_cast<int>(type));
      ASSERT_EQ(list[1].GetDouble(), expected_times.front());
      expected_times.pop();
    }
  };

  validate_events(EventType::kChromeOSPresentationDone,
                  dict.FindListByDottedPath("chrome.global_events"));
  const auto* events_by_buffer =
      (*dict.FindListByDottedPath("views"))[0].GetDict().FindListByDottedPath(
          "buffers");

  ASSERT_TRUE(events_by_buffer);
  ASSERT_EQ(events_by_buffer->size(), 1ul);
  validate_events(EventType::kExoSurfaceCommit,
                  (*events_by_buffer)[0].GetIfList());
}

}  // namespace

}  // namespace arc
