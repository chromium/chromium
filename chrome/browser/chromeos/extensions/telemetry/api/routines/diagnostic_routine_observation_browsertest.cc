// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace {

// An `EventRouterObserver` that runs a callback once a callback is registered
// for the provided `event_name`. This class is used in tests to execute code
// once the callback is properly set up. That way we can make sure to dispatch
// events in the right moment. This class also makes sure that the passed
// callback is only executed once by unregistering itself as an observer in case
// the correct event occurs.
class EventRegistrationObserver : public extensions::EventRouter::Observer {
 public:
  explicit EventRegistrationObserver(std::string event_name,
                                     base::OnceClosure on_event_added,
                                     content::BrowserContext* context)
      : context_(context),
        event_name_(event_name),
        on_event_added_(std::move(on_event_added)) {}
  ~EventRegistrationObserver() override = default;

  void OnListenerAdded(const extensions::EventListenerInfo& details) override {
    if (details.event_name.compare(event_name_) == 0) {
      extensions::EventRouter::Get(context_)->UnregisterObserver(this);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_event_added_));
    }
  }

 private:
  raw_ptr<content::BrowserContext> context_;
  std::string event_name_;
  base::OnceClosure on_event_added_;
};

class TelemetryExtensionDiagnosticRoutineObserverBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseTelemetryExtensionBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    registration_observer_.reset();
    observation_.reset();
    BaseTelemetryExtensionBrowserTest::TearDownOnMainThread();
  }

 protected:
  void SetRoutineObservation() {
    // Use an arbitrary value for `argument_tag_for_legacy_finished_events`.
    DiagnosticRoutineInfo info(
        extension_id(), uuid_, profile(),
        ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory);
    observation_ = std::make_unique<DiagnosticRoutineObservation>(
        info, on_finished_future_.GetCallback(),
        remote_.BindNewPipeAndPassReceiver());
  }

  void SetLegacyFinishedEventRoutineObservation(
      ash::cros_healthd::mojom::RoutineArgument::Tag
          argument_tag_for_legacy_finished_events) {
    DiagnosticRoutineInfo info(extension_id(), uuid_, profile(),
                               argument_tag_for_legacy_finished_events);
    observation_ = std::make_unique<DiagnosticRoutineObservation>(
        info, on_finished_future_.GetCallback(),
        remote_.BindNewPipeAndPassReceiver());
  }

  void RegisterEventObserver(std::string event_name,
                             base::OnceClosure on_event_added) {
    registration_observer_ = std::make_unique<EventRegistrationObserver>(
        event_name, std::move(on_event_added), profile());

    extensions::EventRouter::Get(profile())->RegisterObserver(
        registration_observer_.get(), event_name);
  }

  DiagnosticRoutineInfo WaitForFinishedReport() {
    return on_finished_future_.Take();
  }

  base::Uuid uuid_{base::Uuid::GenerateRandomV4()};
  mojo::Remote<ash::cros_healthd::mojom::RoutineObserver> remote_;

 private:
  base::test::TestFuture<DiagnosticRoutineInfo> on_finished_future_;
  std::unique_ptr<EventRegistrationObserver> registration_observer_;
  std::unique_ptr<DiagnosticRoutineObservation> observation_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineInitialized) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineInitialized::kEventName,
      base::BindLambdaForTesting([this] {
        auto init_state = ash::cros_healthd::mojom::RoutineState::New();
        init_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewInitialized(
                ash::cros_healthd::mojom::RoutineStateInitialized::New());
        init_state->percentage = 0;

        remote_->OnRoutineStateChange(std::move(init_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineInitialized() {
        chrome.os.diagnostics.onRoutineInitialized.addListener((event) => {
          chrome.test.assertEq(event, {
            uuid: "%s",
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineRunning) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineRunning::kEventName,
      base::BindLambdaForTesting([this] {
        auto running_state = ash::cros_healthd::mojom::RoutineState::New();
        running_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewRunning(
                ash::cros_healthd::mojom::RoutineStateRunning::New());
        running_state->percentage = 50;

        remote_->OnRoutineStateChange(std::move(running_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineRunning() {
        chrome.os.diagnostics.onRoutineRunning.addListener((event) => {
          chrome.test.assertEq(event, {
            percentage: 50,
            uuid: "%s",
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineRunningWithNetworkBandwidthInfo) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineRunning::kEventName,
      base::BindLambdaForTesting([this] {
        auto network_bandwidth_info =
            ash::cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::New();
        network_bandwidth_info->type = ash::cros_healthd::mojom::
            NetworkBandwidthRoutineRunningInfo::Type::kDownload;
        network_bandwidth_info->speed_kbps = 100.0;

        auto running_state =
            ash::cros_healthd::mojom::RoutineStateRunning::New();
        running_state->info =
            ash::cros_healthd::mojom::RoutineRunningInfo::NewNetworkBandwidth(
                std::move(network_bandwidth_info));

        auto state = ash::cros_healthd::mojom::RoutineState::New();
        state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewRunning(
                std::move(running_state));
        state->percentage = 50;

        remote_->OnRoutineStateChange(std::move(state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineRunningWithNetworkBandwidthInfo() {
        chrome.os.diagnostics.onRoutineRunning.addListener((event) => {
          chrome.test.assertEq(event, {
            info: {
              "networkBandwidth": {
                "speedKbps": 100.0,
                "type": "download",
              }
            },
            percentage: 50,
            uuid:"%s",
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineWaiting) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineWaiting::kEventName,
      base::BindLambdaForTesting([this] {
        auto waiting_state = ash::cros_healthd::mojom::RoutineState::New();
        waiting_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewWaiting(
                ash::cros_healthd::mojom::RoutineStateWaiting::New(
                    ash::cros_healthd::mojom::RoutineStateWaiting::Reason::
                        kWaitingToBeScheduled,
                    "TEST"));
        waiting_state->percentage = 50;

        remote_->OnRoutineStateChange(std::move(waiting_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineWaiting() {
        chrome.os.diagnostics.onRoutineWaiting.addListener((event) => {
          chrome.test.assertEq(event, {
            message: "TEST",
            percentage: 50,
            reason: "waiting_to_be_scheduled",
            uuid: "%s",
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveLegacyOnMemoryRoutineFinished) {
  SetLegacyFinishedEventRoutineObservation(
      ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory);
  RegisterEventObserver(
      api::os_diagnostics::OnMemoryRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto memtester_result =
            ash::cros_healthd::mojom::MemtesterResult::New();
        memtester_result->passed_items = {
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareDIV,
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareMUL};
        memtester_result->failed_items = {
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareAND,
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareSUB};

        auto memory_detail =
            ash::cros_healthd::mojom::MemoryRoutineDetail::New();
        memory_detail->bytes_tested = 500;
        memory_detail->result = std::move(memtester_result);

        auto finished_detail =
            ash::cros_healthd::mojom::RoutineDetail::NewMemory(
                std::move(memory_detail));

        auto finished_state = ash::cros_healthd::mojom::RoutineState::New();
        finished_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
                ash::cros_healthd::mojom::RoutineStateFinished::New(
                    /*has_passed=*/true, std::move(finished_detail)));
        finished_state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(finished_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnMemoryRoutineFinished() {
        chrome.os.diagnostics.onMemoryRoutineFinished.addListener((event) => {
          chrome.test.assertEq(event, {
            "bytesTested": 500,
            "has_passed": true,
            "result": {
              "failed_items": [
                "compare_and",
                "compare_sub"
              ],
              "passed_items": [
                "compare_div",
                "compare_mul"
              ]
            },
            "uuid":"%s"
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));

  auto info = WaitForFinishedReport();
  EXPECT_EQ(info.extension_id, extension_id());
  EXPECT_EQ(info.uuid, uuid_);
}

// In newer implementation of healthd, a finished volume button routine does not
// contain the routine detail.
IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
    CanObserveLegacyOnVolumeButtonRoutineFinishedWithoutRoutineDetail) {
  SetLegacyFinishedEventRoutineObservation(
      ash::cros_healthd::mojom::RoutineArgument::Tag::kVolumeButton);
  RegisterEventObserver(
      api::os_diagnostics::OnVolumeButtonRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto finished_state = ash::cros_healthd::mojom::RoutineState::New();
        finished_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
                ash::cros_healthd::mojom::RoutineStateFinished::New(
                    /*has_passed=*/true, /*detail=*/nullptr));
        finished_state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(finished_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnVolumeButtonRoutineFinished() {
        chrome.os.diagnostics.onVolumeButtonRoutineFinished.addListener(
          (event) => {
            chrome.test.assertEq(event, {
              "has_passed": true,
              "uuid":"%s"
            });

            chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));

  auto info = WaitForFinishedReport();
  EXPECT_EQ(info.extension_id, extension_id());
  EXPECT_EQ(info.uuid, uuid_);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveLegacyOnFanRoutineFinished) {
  SetLegacyFinishedEventRoutineObservation(
      ash::cros_healthd::mojom::RoutineArgument::Tag::kFan);
  RegisterEventObserver(
      api::os_diagnostics::OnFanRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto fan_detail = ash::cros_healthd::mojom::FanRoutineDetail::New();
        fan_detail->passed_fan_ids = {0};
        fan_detail->failed_fan_ids = {1};
        fan_detail->fan_count_status =
            ash::cros_healthd::mojom::HardwarePresenceStatus::kMatched;

        auto finished_detail = ash::cros_healthd::mojom::RoutineDetail::NewFan(
            std::move(fan_detail));

        auto finished_state = ash::cros_healthd::mojom::RoutineState::New();
        finished_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
                ash::cros_healthd::mojom::RoutineStateFinished::New(
                    /*has_passed=*/true, std::move(finished_detail)));
        finished_state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(finished_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnFanRoutineFinished() {
        chrome.os.diagnostics.onFanRoutineFinished.addListener((event) => {
          chrome.test.assertEq(event, {
            "passed_fan_ids": [0],
            "failed_fan_ids": [1],
            "fan_count_status": "matched",
            "has_passed": true,
            "uuid":"%s"
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));

  auto info = WaitForFinishedReport();
  EXPECT_EQ(info.extension_id, extension_id());
  EXPECT_EQ(info.uuid, uuid_);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineFinishedWithNullDetail) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto finished_state = ash::cros_healthd::mojom::RoutineState::New();
        finished_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
                ash::cros_healthd::mojom::RoutineStateFinished::New(
                    /*has_passed=*/true, /*detail=*/nullptr));
        finished_state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(finished_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineFinishedWithNullDetail() {
        chrome.os.diagnostics.onRoutineFinished.addListener((event) => {
          chrome.test.assertEq(event, {
            "hasPassed": true,
            "uuid":"%s"
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));

  auto info = WaitForFinishedReport();
  EXPECT_EQ(info.extension_id, extension_id());
  EXPECT_EQ(info.uuid, uuid_);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineFinishedWithMemoryDetail) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto memtester_result =
            ash::cros_healthd::mojom::MemtesterResult::New();
        memtester_result->passed_items = {
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareDIV,
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareMUL};
        memtester_result->failed_items = {
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareAND,
            ash::cros_healthd::mojom::MemtesterTestItemEnum::kCompareSUB};

        auto memory_detail =
            ash::cros_healthd::mojom::MemoryRoutineDetail::New();
        memory_detail->bytes_tested = 500;
        memory_detail->result = std::move(memtester_result);

        auto finished_detail =
            ash::cros_healthd::mojom::RoutineDetail::NewMemory(
                std::move(memory_detail));

        auto finished_state = ash::cros_healthd::mojom::RoutineState::New();
        finished_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
                ash::cros_healthd::mojom::RoutineStateFinished::New(
                    /*has_passed=*/true, std::move(finished_detail)));
        finished_state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(finished_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineFinishedWithMemoryDetail() {
        chrome.os.diagnostics.onRoutineFinished.addListener((event) => {
          chrome.test.assertEq(event, {
            "hasPassed": true,
            "detail": {
              "memory": {
                "bytesTested": 500,
                "result": {
                  "failedItems": [
                    "compare_and",
                    "compare_sub"
                  ],
                  "passedItems": [
                    "compare_div",
                    "compare_mul"
                  ]
                },
              }
            },
            "uuid":"%s"
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));

  auto info = WaitForFinishedReport();
  EXPECT_EQ(info.extension_id, extension_id());
  EXPECT_EQ(info.uuid, uuid_);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineFinishedWithFanDetail) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto fan_detail = ash::cros_healthd::mojom::FanRoutineDetail::New();
        fan_detail->passed_fan_ids = {0};
        fan_detail->failed_fan_ids = {1};
        fan_detail->fan_count_status =
            ash::cros_healthd::mojom::HardwarePresenceStatus::kMatched;

        auto finished_detail = ash::cros_healthd::mojom::RoutineDetail::NewFan(
            std::move(fan_detail));

        auto finished_state = ash::cros_healthd::mojom::RoutineState::New();
        finished_state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
                ash::cros_healthd::mojom::RoutineStateFinished::New(
                    /*has_passed=*/true, std::move(finished_detail)));
        finished_state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(finished_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineFinishedWithFanDetail() {
        chrome.os.diagnostics.onRoutineFinished.addListener((event) => {
          chrome.test.assertEq(event, {
            "detail": {
              "fan": {
                "passedFanIds": [0],
                "failedFanIds": [1],
                "fanCountStatus": "matched"
              }
            },
            "hasPassed": true,
            "uuid":"%s"
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));

  auto info = WaitForFinishedReport();
  EXPECT_EQ(info.extension_id, extension_id());
  EXPECT_EQ(info.uuid, uuid_);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineFinishedWithNetworkBandwidthDetail) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto network_bandwidth_detail =
            ash::cros_healthd::mojom::NetworkBandwidthRoutineDetail::New();
        network_bandwidth_detail->download_speed_kbps = 123.0;
        network_bandwidth_detail->upload_speed_kbps = 456.0;

        auto finished_state =
            ash::cros_healthd::mojom::RoutineStateFinished::New();
        finished_state->detail =
            ash::cros_healthd::mojom::RoutineDetail::NewNetworkBandwidth(
                std::move(network_bandwidth_detail));
        finished_state->has_passed = true;

        auto state = ash::cros_healthd::mojom::RoutineState::New();
        state->state_union =
            ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
                std::move(finished_state));
        state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineFinishedWithNetworkBandwidthDetail() {
        chrome.os.diagnostics.onRoutineFinished.addListener((event) => {
          chrome.test.assertEq(event, {
            "detail": {
              "networkBandwidth": {
                "downloadSpeedKbps": 123.0,
                "uploadSpeedKbps": 456.0
              }
            },
            "hasPassed": true,
            "uuid":"%s"
          });

          chrome.test.succeed();
        });
      }
    ]);
  )",
                         uuid_.AsLowercaseString().c_str()));

  auto info = WaitForFinishedReport();
  EXPECT_EQ(info.extension_id, extension_id());
  EXPECT_EQ(info.uuid, uuid_);
}

}  // namespace chromeos
