// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using ::ash::cros_healthd::mojom::EventObserver;

class FakeCrosHealthdAudioObserver
    : public EventObserver,
      public MojoServiceEventsObserverBase<EventObserver> {
 public:
  FakeCrosHealthdAudioObserver()
      : MojoServiceEventsObserverBase<EventObserver>(this) {}

  FakeCrosHealthdAudioObserver(const FakeCrosHealthdAudioObserver&) = delete;
  FakeCrosHealthdAudioObserver& operator=(const FakeCrosHealthdAudioObserver&) =
      delete;

  ~FakeCrosHealthdAudioObserver() override = default;

  void OnEvent(const ash::cros_healthd::mojom::EventInfoPtr info) override {
    MetricData metric_data;
    metric_data.mutable_telemetry_data();
    OnEventObserved(std::move(metric_data));
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 protected:
  void AddObserver() override {
    ash::cros_healthd::ServiceConnection::GetInstance()
        ->GetEventService()
        ->AddEventObserver(ash::cros_healthd::mojom::EventCategoryEnum::kAudio,
                           BindNewPipeAndPassRemote());
  }
};

class MojoServiceEventsObserverBaseTest : public ::testing::Test {
 public:
  MojoServiceEventsObserverBaseTest() = default;

  MojoServiceEventsObserverBaseTest(const MojoServiceEventsObserverBaseTest&) =
      delete;
  MojoServiceEventsObserverBaseTest& operator=(
      const MojoServiceEventsObserverBaseTest&) = delete;

  ~MojoServiceEventsObserverBaseTest() override = default;

  void SetUp() override { ::ash::cros_healthd::FakeCrosHealthd::Initialize(); }

  void TearDown() override { ::ash::cros_healthd::FakeCrosHealthd::Shutdown(); }

  void EmitAudioUnderrunEventForTesting() {
    ::ash::cros_healthd::mojom::AudioEventInfo info;
    info.state = ::ash::cros_healthd::mojom::AudioEventInfo::State::kUnderrun;
    ::ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
        ::ash::cros_healthd::mojom::EventCategoryEnum::kAudio,
        ::ash::cros_healthd::mojom::EventInfo::NewAudioEventInfo(info.Clone()));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(MojoServiceEventsObserverBaseTest, Default) {
  FakeCrosHealthdAudioObserver audio_observer;
  MetricData result_metric_data;
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    result_metric_data = std::move(metric_data);
  });
  audio_observer.SetOnEventObservedCallback(std::move(cb));

  {
    base::RunLoop run_loop;

    audio_observer.SetReportingEnabled(true);
    EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Reporting is enabled.
  ASSERT_TRUE(result_metric_data.has_telemetry_data());

  // Shutdown cros_healthd to simulate crash.
  ash::cros_healthd::FakeCrosHealthd::Shutdown();
  // Restart cros_healthd.
  ash::cros_healthd::FakeCrosHealthd::Initialize();
  audio_observer.FlushForTesting();

  result_metric_data.Clear();
  {
    base::RunLoop run_loop;

    EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Observer reconnected after crash.
  EXPECT_TRUE(result_metric_data.has_telemetry_data());

  result_metric_data.Clear();
  {
    base::RunLoop run_loop;

    audio_observer.SetReportingEnabled(false);
    EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Reporting is disabled.
  EXPECT_FALSE(result_metric_data.has_telemetry_data());
}

}  // namespace
}  // namespace reporting
