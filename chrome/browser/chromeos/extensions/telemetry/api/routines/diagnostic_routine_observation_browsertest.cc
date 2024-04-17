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
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace {

namespace crosapi = ::crosapi::mojom;

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
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
    observation_ = std::make_unique<DiagnosticRoutineObservation>(
        info, on_finished_future_.GetCallback(),
        remote_.BindNewPipeAndPassReceiver());
  }

  void SetLegacyFinishedEventRoutineObservation(
      crosapi::TelemetryDiagnosticRoutineArgument::Tag
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
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineObserver> remote_;

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
        auto init_state = crosapi::TelemetryDiagnosticRoutineState::New();
        init_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
                crosapi::TelemetryDiagnosticRoutineStateInitialized::New());
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
        auto running_state = crosapi::TelemetryDiagnosticRoutineState::New();
        running_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
                crosapi::TelemetryDiagnosticRoutineStateRunning::New());
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
        auto network_bandwidth_info = crosapi::
            TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::New();
        network_bandwidth_info->type =
            crosapi::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::
                Type::kDownload;
        network_bandwidth_info->speed_kbps = 100.0;

        auto running_state =
            crosapi::TelemetryDiagnosticRoutineStateRunning::New();
        running_state->info =
            crosapi::TelemetryDiagnosticRoutineRunningInfo::NewNetworkBandwidth(
                std::move(network_bandwidth_info));

        auto state = crosapi::TelemetryDiagnosticRoutineState::New();
        state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
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
        auto waiting_state = crosapi::TelemetryDiagnosticRoutineState::New();
        waiting_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewWaiting(
                crosapi::TelemetryDiagnosticRoutineStateWaiting::New(
                    crosapi::TelemetryDiagnosticRoutineStateWaiting::Reason::
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
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
  RegisterEventObserver(
      api::os_diagnostics::OnMemoryRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto memtester_result =
            crosapi::TelemetryDiagnosticMemtesterResult::New();
        memtester_result->passed_items = {
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV,
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL};
        memtester_result->failed_items = {
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND,
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB};

        auto memory_detail =
            crosapi::TelemetryDiagnosticMemoryRoutineDetail::New();
        memory_detail->bytes_tested = 500;
        memory_detail->result = std::move(memtester_result);

        auto finished_detail =
            crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
                std::move(memory_detail));

        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
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
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kVolumeButton);
  RegisterEventObserver(
      api::os_diagnostics::OnVolumeButtonRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
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

// In older implementation of healthd, a finished volume button routine contains
// a routine detail.
IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
    CanObserveLegacyOnVolumeButtonRoutineFinishedWithRoutineDetail) {
  SetLegacyFinishedEventRoutineObservation(
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kVolumeButton);
  RegisterEventObserver(
      api::os_diagnostics::OnVolumeButtonRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto volume_button_detail =
            crosapi::TelemetryDiagnosticVolumeButtonRoutineDetail::New();

        auto finished_detail =
            crosapi::TelemetryDiagnosticRoutineDetail::NewVolumeButton(
                std::move(volume_button_detail));

        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
                    /*has_passed=*/true, std::move(finished_detail)));
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
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kFan);
  RegisterEventObserver(
      api::os_diagnostics::OnFanRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto fan_detail = crosapi::TelemetryDiagnosticFanRoutineDetail::New();
        fan_detail->passed_fan_ids = {0};
        fan_detail->failed_fan_ids = {1};
        fan_detail->fan_count_status =
            crosapi::TelemetryDiagnosticHardwarePresenceStatus::kMatched;

        auto finished_detail =
            crosapi::TelemetryDiagnosticRoutineDetail::NewFan(
                std::move(fan_detail));

        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
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
        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
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
            crosapi::TelemetryDiagnosticMemtesterResult::New();
        memtester_result->passed_items = {
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareDIV,
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareMUL};
        memtester_result->failed_items = {
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareAND,
            crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kCompareSUB};

        auto memory_detail =
            crosapi::TelemetryDiagnosticMemoryRoutineDetail::New();
        memory_detail->bytes_tested = 500;
        memory_detail->result = std::move(memtester_result);

        auto finished_detail =
            crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
                std::move(memory_detail));

        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
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

// In older implementation of healthd, a finished volume button routine contains
// a routine detail.
IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineFinishedWithVolumeButtonDetail) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto volume_button_detail =
            crosapi::TelemetryDiagnosticVolumeButtonRoutineDetail::New();

        auto finished_detail =
            crosapi::TelemetryDiagnosticRoutineDetail::NewVolumeButton(
                std::move(volume_button_detail));

        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
                    /*has_passed=*/true, std::move(finished_detail)));
        finished_state->percentage = 100;

        remote_->OnRoutineStateChange(std::move(finished_state));
      }));

  CreateExtensionAndRunServiceWorker(
      base::StringPrintf(R"(
    chrome.test.runTests([
      async function canObserveOnRoutineFinishedWithVolumeButtonDetail() {
        chrome.os.diagnostics.onRoutineFinished.addListener(
          (event) => {
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
                       CanObserveOnRoutineFinishedWithFanDetail) {
  SetRoutineObservation();
  RegisterEventObserver(
      api::os_diagnostics::OnRoutineFinished::kEventName,
      base::BindLambdaForTesting([this] {
        auto fan_detail = crosapi::TelemetryDiagnosticFanRoutineDetail::New();
        fan_detail->passed_fan_ids = {0};
        fan_detail->failed_fan_ids = {1};
        fan_detail->fan_count_status =
            crosapi::TelemetryDiagnosticHardwarePresenceStatus::kMatched;

        auto finished_detail =
            crosapi::TelemetryDiagnosticRoutineDetail::NewFan(
                std::move(fan_detail));

        auto finished_state = crosapi::TelemetryDiagnosticRoutineState::New();
        finished_state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
                crosapi::TelemetryDiagnosticRoutineStateFinished::New(
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
            crosapi::TelemetryDiagnosticNetworkBandwidthRoutineDetail::New();
        network_bandwidth_detail->download_speed_kbps = 123.0;
        network_bandwidth_detail->upload_speed_kbps = 456.0;

        auto finished_state =
            crosapi::TelemetryDiagnosticRoutineStateFinished::New();
        finished_state->detail =
            crosapi::TelemetryDiagnosticRoutineDetail::NewNetworkBandwidth(
                std::move(network_bandwidth_detail));
        finished_state->has_passed = true;

        auto state = crosapi::TelemetryDiagnosticRoutineState::New();
        state->state_union =
            crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
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
