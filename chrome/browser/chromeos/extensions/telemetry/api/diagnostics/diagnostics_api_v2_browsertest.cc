// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace chromeos {

using TelemetryExtensionDiagnosticsApiV2BrowserTest =
    BaseTelemetryExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       IsRoutineArgSupportedParseArgumentsError) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isRoutineArgumentSupported({
              memory: {
                maxTestingMemKib: -1,
              },
            }),
            'Error: Routine arguments are invalid.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       IsRoutineArgSupportedInvalidArguments) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isRoutineArgumentSupported({
              memory: {},
              fan: {},
            }),
            'Error: Routine arguments are invalid.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       IsRoutineArgSupportedApiInternalError) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(0));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isRoutineArgumentSupported({
              memory: {},
            }),
            'Error: API internal error.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       IsRoutineArgSupportedException) {
  auto exception = ash::cros_healthd::mojom::Exception::New();
  exception->debug_message = "TEST_MESSAGE";
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewException(
              std::move(exception)));

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isRoutineArgumentSupported({
              memory: {},
            }),
            'Error: TEST_MESSAGE'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionDiagnosticsApiV2BrowserTest,
    IsRoutineArgSupportedSuccessWithUnrecognizedRoutineName) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewUnsupported(
              ash::cros_healthd::mojom::Unsupported::New("TEST_MESSAGE",
                                                         /*reason=*/nullptr)));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isRoutineArgSupported() {
        const result = await chrome.os.diagnostics.isRoutineArgumentSupported({
            newRoutine: {}
        });

        chrome.test.assertEq(result.status, 'unsupported');

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       IsRoutineArgSupportedSuccess) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewSupported(
              ash::cros_healthd::mojom::Supported::New()));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isRoutineArgSupported() {
        const result = await chrome.os.diagnostics.isRoutineArgumentSupported({
            memory: {}
        });

        chrome.test.assertEq(result.status, 'supported');

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateRoutineInvalidArguments) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createRoutineFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createRoutine({
              memory: {},
              fan: {},
            }),
            'Error: Routine arguments are invalid.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateRoutineSuccessWithUnrecognizedRoutineName) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createUnrecognizedRoutine() {
        const result = await chrome.os.diagnostics.createRoutine({
          newRoutine: {},
        });

        chrome.test.assertTrue(result !== undefined);
        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateRoutineParseArgumentsError) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createRoutineFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createRoutine({
              memory: {
                maxTestingMemKib: -1,
              },
            }),
            'Error: Routine arguments are invalid.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateRoutineCompanionUiNotOpenError) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createRoutineFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createRoutine({
              memory: {}
            }),
            'Error: Companion app UI is not open.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateRoutineSuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createRoutine({
          memory: {},
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyCreateMemoryRoutineCompanionUiNotOpenError) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createMemoryRoutineFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createMemoryRoutine({
              maxTestingMemKib: 42,
            }),
            'Error: Companion app UI is not open.'
        );

        chrome.test.succeed();
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateRoutineResetConnectionResultsInException) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnRoutineCreated() override {
      auto* control =
          ash::cros_healthd::FakeCrosHealthd::Get()
              ->GetRoutineControlForArgumentTag(
                  ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory);
      ASSERT_TRUE(control);
      control->GetReceiver()->ResetWithReason(
          static_cast<int32_t>(
              ash::cros_healthd::mojom::Exception::Reason::kUnsupported),
          "test message");
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createMemoryRoutineResetConnection() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        chrome.os.diagnostics.onRoutineException.addListener(async (status) => {
          chrome.test.assertEq(status, {
            "uuid": await uuid,
            "reason": "unsupported",
            "debugMessage": "test message"
          });

          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createMemoryRoutine({
          maxTestingMemKib: 42,
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       FinishedRoutineIsRemovedSuccess) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnRoutineCreated() override {
      auto* control =
          ash::cros_healthd::FakeCrosHealthd::Get()
              ->GetRoutineControlForArgumentTag(
                  ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory);
      ASSERT_TRUE(control);

      auto memory_detail = ash::cros_healthd::mojom::MemoryRoutineDetail::New();
      memory_detail->result = ash::cros_healthd::mojom::MemtesterResult::New();

      auto finished_state =
          ash::cros_healthd::mojom::RoutineStateFinished::New();
      finished_state->has_passed = true;
      finished_state->detail =
          ash::cros_healthd::mojom::RoutineDetail::NewMemory(
              std::move(memory_detail));

      auto state = ash::cros_healthd::mojom::RoutineState::New();
      state->state_union =
          ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
              std::move(finished_state));
      CHECK_DEREF(control->GetObserver())
          ->OnRoutineStateChange(std::move(state));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createMemoryRoutine() {
        let uuid_resolver;
        let finished_resolver;

        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          uuid_resolver = resolve;
        });

        var on_finished = new Promise((resolve) => {
          finished_resolver = resolve;
        });

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onMemoryRoutineFinished.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          finished_resolver();
          });

        const response = await chrome.os.diagnostics.createMemoryRoutine({
          maxTestingMemKib: 42,
        });
        chrome.test.assertTrue(response !== undefined);
        uuid_resolver(response.uuid);
        await on_finished;
        // Test that we were successful by starting again and failing.
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.startRoutine({
              uuid: response.uuid,
            }),
            'Error: Unknown routine id.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       ClosingAppUiResultsInException) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    explicit TestObserver(TabStripModel* tab_strip_model)
        : tab_strip_model_(CHECK_DEREF(tab_strip_model)) {}

    void OnRoutineCreated() override {
      // Closing the tab results in an exception.
      ASSERT_TRUE(tab_strip_model_->ContainsIndex(0));
      tab_strip_model_->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
    }

   private:
    const raw_ref<TabStripModel> tab_strip_model_;
  } observer(browser()->tab_strip_model());
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  // Open a second tab so that we don't close the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://version"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function closingAppUiResultsInException() {
        chrome.os.diagnostics.onRoutineException.addListener(async (status) => {
          chrome.test.assertEq(status, {
            "reason": "app_ui_closed",
          });

          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createMemoryRoutine({
          maxTestingMemKib: 42,
        });
        chrome.test.assertTrue(response !== undefined);
          }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyCreateMemoryRoutineSuccess) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnRoutineCreated() override {
      auto* control =
          ash::cros_healthd::FakeCrosHealthd::Get()
              ->GetRoutineControlForArgumentTag(
                  ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory);
      ASSERT_TRUE(control);

      auto memtester_result = ash::cros_healthd::mojom::MemtesterResult::New();
      memtester_result->passed_items = {
          ash::cros_healthd::mojom::MemtesterTestItemEnum::k16BitWrites};
      memtester_result->failed_items = {
          ash::cros_healthd::mojom::MemtesterTestItemEnum::k8BitWrites};

      auto memory_detail = ash::cros_healthd::mojom::MemoryRoutineDetail::New();
      memory_detail->bytes_tested = 42;
      memory_detail->result = std::move(memtester_result);

      auto finished_state =
          ash::cros_healthd::mojom::RoutineStateFinished::New();
      finished_state->has_passed = true;
      finished_state->detail =
          ash::cros_healthd::mojom::RoutineDetail::NewMemory(
              std::move(memory_detail));

      auto state = ash::cros_healthd::mojom::RoutineState::New();
      state->state_union =
          ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
              std::move(finished_state));
      CHECK_DEREF(control->GetObserver())
          ->OnRoutineStateChange(std::move(state));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createMemoryRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        let onInitCalled = false;
        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          onInitCalled = true;
        });

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onMemoryRoutineFinished.addListener(
          async (status) => {
          chrome.test.assertEq(status, {
            "bytesTested": 42,
            "has_passed": true,
            "result": {
                "failed_items": ["eight_bit_writes"],
                "passed_items": ["sixteen_bit_writes"]
            },
            "uuid": await uuid,
          });
          chrome.test.assertTrue(onInitCalled);

          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createMemoryRoutine({
          maxTestingMemKib: 42,
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyCreateMemoryRoutineNoOptionalConfigSuccess) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnRoutineCreated() override {
      auto* control =
          ash::cros_healthd::FakeCrosHealthd::Get()
              ->GetRoutineControlForArgumentTag(
                  ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory);
      ASSERT_TRUE(control);

      auto memtester_result = ash::cros_healthd::mojom::MemtesterResult::New();
      memtester_result->passed_items = {
          ash::cros_healthd::mojom::MemtesterTestItemEnum::k16BitWrites};
      memtester_result->failed_items = {
          ash::cros_healthd::mojom::MemtesterTestItemEnum::k8BitWrites};

      auto memory_detail = ash::cros_healthd::mojom::MemoryRoutineDetail::New();
      memory_detail->bytes_tested = 42;
      memory_detail->result = std::move(memtester_result);

      auto finished_state =
          ash::cros_healthd::mojom::RoutineStateFinished::New();
      finished_state->has_passed = true;
      finished_state->detail =
          ash::cros_healthd::mojom::RoutineDetail::NewMemory(
              std::move(memory_detail));

      auto state = ash::cros_healthd::mojom::RoutineState::New();
      state->state_union =
          ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
              std::move(finished_state));
      CHECK_DEREF(control->GetObserver())
          ->OnRoutineStateChange(std::move(state));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createMemoryRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        let onInitCalled = false;
        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          onInitCalled = true;
        });

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onMemoryRoutineFinished.addListener(
          async (status) => {
          chrome.test.assertEq(status, {
            "bytesTested": 42,
            "has_passed": true,
            "result": {
                "failed_items": ["eight_bit_writes"],
                "passed_items": ["sixteen_bit_writes"]
            },
            "uuid": await uuid,
          });
          chrome.test.assertTrue(onInitCalled);

          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createMemoryRoutine({});
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       StartRoutineUnknownUuidError) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function startRoutineFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.startRoutine({
              uuid: '123',
            }),
            'Error: Unknown routine id.'
        );

        chrome.test.succeed();
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       StartRoutineSuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createMemoryRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        let onInitCalled = false;
        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
            chrome.test.assertEq(status.uuid, await uuid);
            onInitCalled = true;
          }
        );

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onRoutineRunning.addListener(
          async (status) => {
            chrome.test.assertEq(status.uuid, await uuid);
            chrome.test.assertTrue(onInitCalled);

            chrome.test.succeed();
          }
        );

        const response = await chrome.os.diagnostics.createMemoryRoutine({
          maxTestingMemKib: 42,
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);

        await chrome.os.diagnostics.startRoutine({ uuid: response.uuid });
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CancelRoutineSuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function cancelRoutine() {
        const response = await chrome.os.diagnostics.createMemoryRoutine({
          maxTestingMemKib: 42,
        });
        chrome.test.assertTrue(response !== undefined);

        // Start the routine.
        await chrome.os.diagnostics.startRoutine({ uuid: response.uuid });

        // Now cancel the routine.
        await chrome.os.diagnostics.cancelRoutine({ uuid: response.uuid });

        // Test that we were successful by starting again and failing.
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.startRoutine({
              uuid: response.uuid,
            }),
            'Error: Unknown routine id.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsMemoryRoutineArgSupportedApiInternalError) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(0));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isMemoryRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isMemoryRoutineArgumentSupported({
              maxTestingMemKib: 42,
            }),
            'Error: API internal error.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsMemoryRoutineArgSupportedException) {
  auto exception = ash::cros_healthd::mojom::Exception::New();
  exception->debug_message = "TEST_MESSAGE";
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewException(
              std::move(exception)));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isMemoryRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isMemoryRoutineArgumentSupported({
              maxTestingMemKib: 42,
            }),
            'Error: TEST_MESSAGE'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsMemoryRoutineArgSupportedSuccess) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewSupported(
              ash::cros_healthd::mojom::Supported::New()));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isMemoryRoutineArgSupported() {
        const result = await chrome.os.diagnostics.
          isMemoryRoutineArgumentSupported({
            maxTestingMemKib: 42,
        });

        chrome.test.assertEq(result.status, 'supported');

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionDiagnosticsApiV2BrowserTest,
    LegacyIsVolumeButtonRoutineArgSupportedApiInternalError) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(0));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isVolumeButtonRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isVolumeButtonRoutineArgumentSupported({
              button_type: "volume_up",
              timeout_seconds: 10,
            }),
            'Error: API internal error.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsVolumeButtonRoutineArgSupportedException) {
  auto exception = ash::cros_healthd::mojom::Exception::New();
  exception->debug_message = "TEST_MESSAGE";
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewException(
              std::move(exception)));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isVolumeButtonRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isVolumeButtonRoutineArgumentSupported({
              button_type: "volume_up",
              timeout_seconds: 10,
            }),
            'Error: TEST_MESSAGE'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsVolumeButtonRoutineArgSupportedSuccess) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewSupported(
              ash::cros_healthd::mojom::Supported::New()));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isVolumeButtonRoutineArgSupported() {
        const result = await chrome.os.diagnostics.
          isVolumeButtonRoutineArgumentSupported({
            button_type: "volume_up",
            timeout_seconds: 10,
        });

        chrome.test.assertEq(result.status, 'supported');

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyCreateVolumeButtonRoutineCompanionUiNotOpenError) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createVolumeButtonRoutineFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createVolumeButtonRoutine({
              button_type: "volume_up",
              timeout_seconds: 10,
            }),
            'Error: Companion app UI is not open.'
        );

        chrome.test.succeed();
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyCreateVolumeButtonRoutineSuccess) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnRoutineCreated() override {
      auto* control = ash::cros_healthd::FakeCrosHealthd::Get()
                          ->GetRoutineControlForArgumentTag(
                              ash::cros_healthd::mojom::RoutineArgument::Tag::
                                  kVolumeButton);
      ASSERT_TRUE(control);

      auto finished_state =
          ash::cros_healthd::mojom::RoutineStateFinished::New();
      finished_state->has_passed = true;
      finished_state->detail = nullptr;

      auto state = ash::cros_healthd::mojom::RoutineState::New();
      state->state_union =
          ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
              std::move(finished_state));
      CHECK_DEREF(control->GetObserver())
          ->OnRoutineStateChange(std::move(state));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createVolumeButtonRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        let onInitCalled = false;
        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          onInitCalled = true;
        });

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onVolumeButtonRoutineFinished.addListener(
          async (status) => {
          chrome.test.assertEq(status, {
            "has_passed": true,
            "uuid": await uuid,
          });
          chrome.test.assertTrue(onInitCalled);

          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createVolumeButtonRoutine({
          button_type: "volume_up",
          timeout_seconds: 10,
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsFanRoutineArgSupportedApiInternalError) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(0));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isFanRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isFanRoutineArgumentSupported({
            }),
            'Error: API internal error.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsFanRoutineArgSupportedException) {
  auto exception = ash::cros_healthd::mojom::Exception::New();
  exception->debug_message = "TEST_MESSAGE";
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewException(
              std::move(exception)));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isFanRoutineArgSupported() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.isFanRoutineArgumentSupported({
            }),
            'Error: TEST_MESSAGE'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyIsFanRoutineArgSupportedSuccess) {
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetIsRoutineArgumentSupportedResponseForTesting(
          ash::cros_healthd::mojom::SupportStatus::NewSupported(
              ash::cros_healthd::mojom::Supported::New()));
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function isFanRoutineArgSupported() {
        const result = await chrome.os.diagnostics.
          isFanRoutineArgumentSupported({
        });

        chrome.test.assertEq(result.status, 'supported');

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyCreateFanRoutineCompanionUiNotOpenError) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createFanRoutineFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.createFanRoutine({
            }),
            'Error: Companion app UI is not open.'
        );

        chrome.test.succeed();
      }
    ]);
    )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       LegacyCreateFanRoutineSuccess) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnRoutineCreated() override {
      auto* control =
          ash::cros_healthd::FakeCrosHealthd::Get()
              ->GetRoutineControlForArgumentTag(
                  ash::cros_healthd::mojom::RoutineArgument::Tag::kFan);
      ASSERT_TRUE(control);

      auto fan_detail = ash::cros_healthd::mojom::FanRoutineDetail::New();
      fan_detail->passed_fan_ids = {0};
      fan_detail->failed_fan_ids = {1};
      fan_detail->fan_count_status =
          ash::cros_healthd::mojom::HardwarePresenceStatus::kMatched;

      auto finished_state =
          ash::cros_healthd::mojom::RoutineStateFinished::New();
      finished_state->has_passed = true;
      finished_state->detail = ash::cros_healthd::mojom::RoutineDetail::NewFan(
          std::move(fan_detail));

      auto state = ash::cros_healthd::mojom::RoutineState::New();
      state->state_union =
          ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
              std::move(finished_state));
      CHECK_DEREF(control->GetObserver())
          ->OnRoutineStateChange(std::move(state));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createFanRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        let onInitCalled = false;
        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          onInitCalled = true;
        });

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onFanRoutineFinished.addListener(
          async (status) => {
          chrome.test.assertEq(status, {
            "has_passed": true,
            "uuid": await uuid,
            "failed_fan_ids":[1],
            "passed_fan_ids":[0],
            "fan_count_status": "matched",
          });
          chrome.test.assertTrue(onInitCalled);

          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createFanRoutine({
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       ReplyToRoutineInquiryUnknownUuidError) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function replyToRoutineInquiryFail() {
        await chrome.test.assertPromiseRejects(
            chrome.os.diagnostics.replyToRoutineInquiry({
              uuid: '123',
              reply: {},
            }),
            'Error: Unknown routine id.'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       ReplyToRoutineInquirySuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createMemoryRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
            chrome.test.assertEq(status.uuid, await uuid);
          }
        );

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onRoutineRunning.addListener(
          async (status) => {
            chrome.test.assertEq(status.uuid, await uuid);

            await chrome.os.diagnostics.replyToRoutineInquiry({
              uuid: response.uuid,
              reply: {
                checkLedLitUpState: {
                  state: "correct_color",
                }
              },
            });

            chrome.test.succeed();
          }
        );

        const response = await chrome.os.diagnostics.createMemoryRoutine({});
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);

        await chrome.os.diagnostics.startRoutine({ uuid: response.uuid });
      }
    ]);
  )");

  auto* control =
      ash::cros_healthd::FakeCrosHealthd::Get()
          ->GetRoutineControlForArgumentTag(
              ash::cros_healthd::mojom::RoutineArgument::Tag::kMemory);
  ASSERT_TRUE(control);

  // Ensures InquiryReply() is processed.
  control->GetReceiver()->FlushForTesting();
  auto reply = control->GetLastInquiryReply();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(reply->is_check_led_lit_up_state());
  EXPECT_EQ(
      reply->get_check_led_lit_up_state()->state,
      ash::cros_healthd::mojom::CheckLedLitUpStateReply::State::kCorrectColor);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateLedLitUpRoutineSuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createRoutine({
          ledLitUp: {
            name: 'battery',
            color: 'red',
          },
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");

  EXPECT_TRUE(
      ash::cros_healthd::FakeCrosHealthd::Get()
          ->GetRoutineControlForArgumentTag(
              ash::cros_healthd::mojom::RoutineArgument::Tag::kLedLitUp));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateKeyboardBacklightRoutineSuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createRoutine({
          keyboardBacklight: {},
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");

  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->GetRoutineControlForArgumentTag(
                      ash::cros_healthd::mojom::RoutineArgument::Tag::
                          kKeyboardBacklight));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       ReplyToKeyboardBacklightRoutineInquirySuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
            chrome.test.assertEq(status.uuid, await uuid);
          }
        );

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onRoutineRunning.addListener(
          async (status) => {
            chrome.test.assertEq(status.uuid, await uuid);

            await chrome.os.diagnostics.replyToRoutineInquiry({
              uuid: response.uuid,
              reply: {
                checkKeyboardBacklightState: {
                  state: "ok",
                }
              },
            });

            chrome.test.succeed();
          }
        );

        const response = await chrome.os.diagnostics.createRoutine({
          keyboardBacklight: {},
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);

        await chrome.os.diagnostics.startRoutine({ uuid: response.uuid });
      }
    ]);
  )");

  auto* control = ash::cros_healthd::FakeCrosHealthd::Get()
                      ->GetRoutineControlForArgumentTag(
                          ash::cros_healthd::mojom::RoutineArgument::Tag::
                              kKeyboardBacklight);
  ASSERT_TRUE(control);

  // Ensures InquiryReply() is processed.
  control->GetReceiver()->FlushForTesting();
  auto reply = control->GetLastInquiryReply();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(reply->is_check_keyboard_backlight_state());
  EXPECT_EQ(
      reply->get_check_keyboard_backlight_state()->state,
      ash::cros_healthd::mojom::CheckKeyboardBacklightStateReply::State::kOk);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateNetworkBandwidthRoutineSuccess) {
  class TestObserver : public ash::cros_healthd::FakeCrosHealthd::Observer {
   public:
    void OnRoutineCreated() override {
      auto* control = ash::cros_healthd::FakeCrosHealthd::Get()
                          ->GetRoutineControlForArgumentTag(
                              ash::cros_healthd::mojom::RoutineArgument::Tag::
                                  kNetworkBandwidth);
      ASSERT_TRUE(control);

      auto network_bandwidth_detail =
          ash::cros_healthd::mojom::NetworkBandwidthRoutineDetail::New();
      network_bandwidth_detail->download_speed_kbps = 123.0;
      network_bandwidth_detail->upload_speed_kbps = 456.0;

      auto finished_state =
          ash::cros_healthd::mojom::RoutineStateFinished::New();
      finished_state->has_passed = true;
      finished_state->detail =
          ash::cros_healthd::mojom::RoutineDetail::NewNetworkBandwidth(
              std::move(network_bandwidth_detail));

      auto state = ash::cros_healthd::mojom::RoutineState::New();
      state->state_union =
          ash::cros_healthd::mojom::RoutineStateUnion::NewFinished(
              std::move(finished_state));
      CHECK_DEREF(control->GetObserver())
          ->OnRoutineStateChange(std::move(state));
    }
  } observer;
  base::ScopedObservation<ash::cros_healthd::FakeCrosHealthd,
                          ash::cros_healthd::FakeCrosHealthd::Observer>
      observation_{&observer};
  observation_.Observe(ash::cros_healthd::FakeCrosHealthd::Get());

  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createNetworkBandwidthRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        let onInitCalled = false;
        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          onInitCalled = true;
        });

        // Only resolve the test once we got the final event.
        chrome.os.diagnostics.onRoutineFinished.addListener(
          async (status) => {
          chrome.test.assertEq(status, {
            "detail": {
              "networkBandwidth": {
                "downloadSpeedKbps": 123.0,
                "uploadSpeedKbps": 456.0
              }
            },
            "hasPassed": true,
            "uuid": await uuid
          });
          chrome.test.assertTrue(onInitCalled);
          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createRoutine({
          networkBandwidth: {},
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateCameraFrameAnalysisRoutineSuccess) {
  OpenAppUiAndMakeItSecure();

  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
       async function createRoutine() {
        let resolver;
        // Set later once the routine was created.
        var uuid = new Promise((resolve) => {
          resolver = resolve;
        });

        chrome.os.diagnostics.onRoutineInitialized.addListener(
          async (status) => {
          chrome.test.assertEq(status.uuid, await uuid);
          chrome.test.succeed();
        });

        const response = await chrome.os.diagnostics.createRoutine({
          cameraFrameAnalysis: {},
        });
        chrome.test.assertTrue(response !== undefined);
        resolver(response.uuid);
      }
    ]);
  )");

  EXPECT_TRUE(ash::cros_healthd::FakeCrosHealthd::Get()
                  ->GetRoutineControlForArgumentTag(
                      ash::cros_healthd::mojom::RoutineArgument::Tag::
                          kCameraFrameAnalysis));
}

class NoExtraPermissionTelemetryExtensionDiagnosticsApiV2BrowserTest
    : public TelemetryExtensionDiagnosticsApiV2BrowserTest {
 public:
  NoExtraPermissionTelemetryExtensionDiagnosticsApiV2BrowserTest() = default;

 protected:
  std::string GetManifestFile(const std::string& manifest_key,
                              const std::string& matches_origin) override {
    return base::StringPrintf(R"(
      {
        "key": "%s",
        "name": "Test Telemetry Extension",
        "version": "1",
        "manifest_version": 3,
        "chromeos_system_extension": {},
        "background": {
          "service_worker": "sw.js"
        },
        "permissions": [ "os.diagnostics" ],
        "externally_connectable": {
          "matches": [
            "%s"
          ]
        },
        "options_page": "options.html"
      }
    )",
                              manifest_key.c_str(), matches_origin.c_str());
  }
};

IN_PROC_BROWSER_TEST_F(
    NoExtraPermissionTelemetryExtensionDiagnosticsApiV2BrowserTest,
    NetworkBandwidthRoutineNoPermissionFail) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      async function createNetworkBandwidthRoutineNoPermission() {
        await chrome.test.assertPromiseRejects(
          chrome.os.diagnostics.createRoutine({
            networkBandwidth: {},
          }),
          'Error: Unauthorized access to ' +
          'chrome.os.diagnostics.CreateRoutine with networkBandwidth ' +
          'argument. Extension doesn\'t have the permission.'
        );
        chrome.test.succeed();
      }
    ]);
  )");
}

}  // namespace chromeos
