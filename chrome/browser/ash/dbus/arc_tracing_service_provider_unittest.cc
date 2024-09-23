// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/arc_tracing_service_provider.h"

#include <memory>
#include <utility>

#include "ash/components/arc/test/arc_task_window_builder.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/tracing/overview_tracing_handler.h"
#include "chrome/browser/ash/arc/tracing/test/overview_tracing_test_base.h"
#include "chrome/browser/ash/arc/tracing/test/overview_tracing_test_handler.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/exo/surface.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;

namespace ash {
namespace {

class TestProvider : public ArcTracingServiceProvider {
 public:
  // Supplies the handler used for the next trace, used by tests to prepare the
  // handler before the trace begins.
  arc::OverviewTracingTestHandler* GetOrCreateNextHandler() {
    if (!next_handler_) {
      next_handler_ = std::make_unique<arc::OverviewTracingTestHandler>(
          arc::OverviewTracingHandler::ArcWindowFocusChangeCb());
    }
    return next_handler_.get();
  }

 private:
  std::unique_ptr<arc::OverviewTracingHandler> NewHandler() override {
    // Make sure a handler is ready to go - ignore return.
    GetOrCreateNextHandler();

    return std::move(next_handler_);
  }

  std::unique_ptr<arc::OverviewTracingTestHandler> next_handler_;
};

class ArcTracingServiceProviderTest : public arc::OverviewTracingTestBase {
 protected:
  void SetUp() override {
    arc::OverviewTracingTestBase::SetUp();

    chromeos::MissiveClient::InitializeFake();

    provider_ = std::make_unique<TestProvider>();

    CHECK(trace_outdir_.CreateUniqueTempDir());
    provider_->set_trace_outdir_for_testing(trace_outdir_.GetPath());

    // Set arbitrary times for tests which fail to start a trace or don't
    // attempt to - we don't actually use the times because a model file is not
    // generated, but at least make the times non-zero.
    auto* first_handler = provider_->GetOrCreateNextHandler();
    first_handler->set_now(
        base::Time::FromSecondsSinceUnixEpoch(1'700'009'000));
    first_handler->set_trace_time_base(
        base::Time::FromSecondsSinceUnixEpoch(1'700'000'000));
  }

  void SetupForRequest(std::string_view method_name) {
    test_helper_.SetUp(arc::tracing::kArcTracingServiceName,
                       dbus::ObjectPath(arc::tracing::kArcTracingServicePath),
                       arc::tracing::kArcTracingInterfaceName,
                       std::string(method_name), provider_.get());
  }

  std::string InvokeStartMethod(double max_time_seconds) {
    SetupForRequest(arc::tracing::kArcTracingStartMethod);
    dbus::MethodCall call{arc::tracing::kArcTracingInterfaceName,
                          arc::tracing::kArcTracingStartMethod};
    dbus::MessageWriter writer{&call};
    writer.AppendDouble(max_time_seconds);
    auto response = test_helper_.CallMethod(&call);

    dbus::MessageReader reader{response.get()};
    std::string response_msg;
    CHECK(reader.PopString(&response_msg));
    test_helper_.TearDown();
    return response_msg;
  }

  std::string InvokeGetStatusMethod() {
    SetupForRequest(arc::tracing::kArcTracingGetStatusMethod);
    dbus::MethodCall call{arc::tracing::kArcTracingInterfaceName,
                          arc::tracing::kArcTracingGetStatusMethod};
    dbus::MessageWriter writer{&call};
    auto response = test_helper_.CallMethod(&call);

    dbus::MessageReader reader{response.get()};
    std::vector<std::string> response_msgs;
    std::string msg;
    while (reader.PopString(&msg)) {
      response_msgs.emplace_back(std::move(msg));
    }
    test_helper_.TearDown();
    return base::JoinString(response_msgs, "\n");
  }

  void TearDown() override {
    provider_.reset();
    chromeos::MissiveClient::Shutdown();
    arc::OverviewTracingTestBase::TearDown();
  }

  std::unique_ptr<TestProvider> provider_;
  base::ScopedTempDir trace_outdir_;
  ServiceProviderTestHelper test_helper_;
};

// The times and timezones in each test below were chosen arbitrarily to
// verify we are formatting the times via the correct API and not ignoring
// timezone settings.

TEST_F(ArcTracingServiceProviderTest, StartTraceAndGetStatus) {
  SetTimeZone("Asia/Chongqing");
  auto* handler = provider_->GetOrCreateNextHandler();

  base::Time time_base;
  ASSERT_TRUE(base::Time::FromString("2023-11-15 08:43:20 +0800", &time_base));
  handler->set_now(time_base + base::Seconds(2));
  handler->set_trace_time_base(time_base);

  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s)
                        .BuildOwnsNativeWidget();

  auto max_time = base::Seconds(5);
  arc_widget->Show();
  ASSERT_EQ("Trace started", InvokeStartMethod(max_time.InSecondsF()));

  EXPECT_EQ("Trace started", InvokeGetStatusMethod());

  handler->StartTracingOnControllerRespond();

  // Present 60 frames per second until end of trace time.
  auto frame_time = base::Microseconds(16'666);
  CommitAndPresentFrames(handler, &s, max_time / frame_time, frame_time);

  // Fast forward past the max tracing interval. This will stop the trace at the
  // end of the fast-forward, which is 400ms after the timeout. There are no
  // events here, so this does not add to the trace duration, so it does not
  // deflate the FPS measurement.
  FastForwardClockAndTaskQueue(handler, base::Milliseconds(400));

  // Pass results from trace controller to handler.
  handler->StopTracingOnControllerRespond(
      std::make_unique<std::string>(arc::kBasicSystrace));

  task_environment()->RunUntilIdle();

  re2::RE2 expected_msg_re{
      base::StrCat({"Trace started\n", "Building model...\n",
                    "Tracing model is ready: ", trace_outdir_.GetPath().value(),
                    "/overview_tracing_arctaskwindowdefaulttitle_2023-11-15_08-"
                    "43-22.json - ",
                    "perceived FPS=([0-9.]+), ", "duration=([0-9.]+)s"})};

  double fps, duration;
  std::string actual_msg = InvokeGetStatusMethod();

  ASSERT_TRUE(RE2::FullMatch(actual_msg, expected_msg_re, &fps, &duration))
      << actual_msg;
  EXPECT_NEAR(fps, 60, 0.5);
  EXPECT_NEAR(duration, 5, 0.2);
}

TEST_F(ArcTracingServiceProviderTest, StopTraceByLosingFocus) {
  SetTimeZone("Europe/Madrid");
  auto* handler = provider_->GetOrCreateNextHandler();
  base::Time time_base;
  ASSERT_TRUE(base::Time::FromString("2024-01-12 12:30:00 +0100", &time_base));
  handler->set_now(time_base + base::Hours(1));
  handler->set_trace_time_base(time_base);

  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s)
                        .BuildOwnsNativeWidget();

  auto max_time = base::Seconds(5);
  arc_widget->Show();
  ASSERT_EQ("Trace started", InvokeStartMethod(max_time.InSecondsF()));

  EXPECT_EQ("Trace started", InvokeGetStatusMethod());

  handler->StartTracingOnControllerRespond();

  // Present frames at 24 fps until 1 second before end of trace. This alone
  // does not stop the test since we don't hit the time limit.
  const auto frame_time = base::Milliseconds(42);
  CommitAndPresentFrames(
      handler, &s, (max_time - base::Seconds(1)) / frame_time, frame_time);

  // Minimizing window will lose focus and stop the trace.
  arc_widget->Minimize();

  // Pass results from trace controller to handler.
  handler->StopTracingOnControllerRespond(
      std::make_unique<std::string>(arc::kBasicSystrace));

  task_environment()->RunUntilIdle();

  re2::RE2 expected_msg_re{
      base::StrCat({"Trace started\n", "Building model...\n",
                    "Tracing model is ready: ", trace_outdir_.GetPath().value(),
                    "/overview_tracing_arctaskwindowdefaulttitle_2024-01-12_13-"
                    "30-00.json - ",
                    "perceived FPS=([0-9.]+), ", "duration=([0-9.]+)s"})};

  double fps, duration;
  std::string actual_msg = InvokeGetStatusMethod();

  ASSERT_TRUE(RE2::FullMatch(actual_msg, expected_msg_re, &fps, &duration))
      << actual_msg;
  EXPECT_NEAR(fps, 24, 0.5);
  EXPECT_NEAR(duration, 4, 0.2);
}

TEST_F(ArcTracingServiceProviderTest, FailedStart) {
  exo::Surface s;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&s)
                        .BuildOwnsNativeWidget();
  arc_widget->ShowInactive();

  ASSERT_EQ("ARC window isn't active", InvokeStartMethod(5));

  EXPECT_EQ("", InvokeGetStatusMethod());
}

TEST_F(ArcTracingServiceProviderTest, FailedStartDueToOneExtraWindow) {
  exo::Surface arc_surface, extra_surface;
  auto arc_widget = arc::ArcTaskWindowBuilder()
                        .SetTaskId(22)
                        .SetPackageName("org.funstuff.client")
                        .SetShellRootSurface(&arc_surface)
                        .BuildOwnsNativeWidget();
  auto extra_widget = arc::ArcTaskWindowBuilder()
                          .SetTaskId(2244)
                          .SetPackageName("net.productive.notfun")
                          .SetShellRootSurface(&arc_surface)
                          .SetTitle("Looking busy")
                          .BuildOwnsNativeWidget();

  extra_widget->Show();
  arc_widget->Show();

  provider_->GetOrCreateNextHandler()->set_non_trace_app_windows(
      {extra_widget->GetNativeWindow()});
  ASSERT_EQ(
      "Extra windows are open. Close them and try the trace again: |Looking "
      "busy|",
      InvokeStartMethod(/*max_time_seconds=*/5));
}

TEST_F(ArcTracingServiceProviderTest, FailedStartDueToTwoExtraWindows) {
  constexpr int kWidgetCount = 3;
  // Note the trace widget is shown last, so it is active.
  constexpr int kTraceWidget = 2;
  std::array<exo::Surface, kWidgetCount> surfaces;
  std::array<std::unique_ptr<views::Widget>, kWidgetCount> widgets;
  arc::OverviewTracingHandler::AppWindowList non_trace_windows;

  for (int i = 0; i < kWidgetCount; i++) {
    widgets[i] = arc::ArcTaskWindowBuilder()
                     .SetTaskId(220 + i)
                     .SetPackageName("org.funstuff.client")
                     .SetShellRootSurface(&surfaces[i])
                     .SetTitle(base::StringPrintf("arc window %d", i))
                     .BuildOwnsNativeWidget();
    widgets[i]->Show();
    if (i != kTraceWidget) {
      non_trace_windows.emplace_back(widgets[i]->GetNativeWindow());
    }
  }

  provider_->GetOrCreateNextHandler()->set_non_trace_app_windows(
      std::move(non_trace_windows));
  ASSERT_EQ(
      "Extra windows are open. Close them and try the trace again: |arc window "
      "0|, |arc window 1|",
      InvokeStartMethod(/*max_time_seconds=*/42.42));
}

}  // namespace
}  // namespace ash
