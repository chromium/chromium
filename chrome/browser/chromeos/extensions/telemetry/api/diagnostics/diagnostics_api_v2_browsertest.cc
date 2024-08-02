// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service_factory.h"
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

class TelemetryExtensionDiagnosticsApiV2BrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  TelemetryExtensionDiagnosticsApiV2BrowserTest() = default;

  ~TelemetryExtensionDiagnosticsApiV2BrowserTest() override = default;

  TelemetryExtensionDiagnosticsApiV2BrowserTest(
      const TelemetryExtensionDiagnosticsApiV2BrowserTest&) = delete;
  TelemetryExtensionDiagnosticsApiV2BrowserTest& operator=(
      const TelemetryExtensionDiagnosticsApiV2BrowserTest&) = delete;

  void SetUpOnMainThread() override {
    BaseTelemetryExtensionBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_routines_service_ = new FakeDiagnosticRoutinesService();
    fake_routines_service_factory_.SetCreateInstanceResponse(
        std::unique_ptr<FakeDiagnosticRoutinesService>(
            fake_routines_service_.get()));

    ash::TelemetryDiagnosticsRoutineServiceAsh::Factory::SetForTesting(
        &fake_routines_service_factory_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    fake_routines_service_ = std::make_unique<FakeDiagnosticRoutinesService>();
    // Replace the production RoutineService with a fake for testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_routines_service_->receiver().BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_routines_service_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    BaseTelemetryExtensionBrowserTest::TearDownOnMainThread();
  }

 protected:
  FakeDiagnosticRoutinesService& fake_service() {
    return CHECK_DEREF(fake_routines_service_.get());
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<FakeDiagnosticRoutinesService> fake_routines_service_;
  FakeDiagnosticRoutinesServiceFactory fake_routines_service_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<FakeDiagnosticRoutinesService> fake_routines_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewUnmappedUnionField(0));
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
  auto exception = crosapi::TelemetryExtensionException::New();
  exception->debug_message = "TEST_MESSAGE";
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewException(
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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewUnsupported(
          crosapi::TelemetryExtensionUnsupported::New("TEST_MESSAGE",
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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewSupported(
          crosapi::TelemetryExtensionSupported::New()));
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
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
    ASSERT_TRUE(control);

    control->receiver().ResetWithReason(
        static_cast<uint32_t>(
            crosapi::TelemetryExtensionException::Reason::kUnsupported),
        "test message");
  }));

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
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
    ASSERT_TRUE(control);

    auto mem_detail = crosapi::TelemetryDiagnosticMemoryRoutineDetail::New();
    mem_detail->result = crosapi::TelemetryDiagnosticMemtesterResult::New();

    auto finished_state =
        crosapi::TelemetryDiagnosticRoutineStateFinished::New();
    finished_state->detail =
        crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
            std::move(mem_detail));
    finished_state->has_passed = true;

    auto state = crosapi::TelemetryDiagnosticRoutineState::New();
    state->state_union =
        crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
            std::move(finished_state));

    control->SetState(std::move(state));
  }));

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
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    // Closing the tab results in an exception.
    ASSERT_TRUE(browser()->tab_strip_model()->ContainsIndex(0));
    browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                     TabCloseTypes::CLOSE_NONE);
  }));
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
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
    ASSERT_TRUE(control);

    auto memtester_result = crosapi::TelemetryDiagnosticMemtesterResult::New();
    memtester_result->passed_items = {
        crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSixteenBitWrites};
    memtester_result->failed_items = {
        crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites};

    auto memory_detail = crosapi::TelemetryDiagnosticMemoryRoutineDetail::New();
    memory_detail->bytes_tested = 42;
    memory_detail->result = std::move(memtester_result);

    auto finished_state =
        crosapi::TelemetryDiagnosticRoutineStateFinished::New();
    finished_state->detail =
        crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
            std::move(memory_detail));
    finished_state->has_passed = true;

    auto state = crosapi::TelemetryDiagnosticRoutineState::New();
    state->state_union =
        crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
            std::move(finished_state));

    control->SetState(std::move(state));
  }));

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
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
    ASSERT_TRUE(control);

    auto memtester_result = crosapi::TelemetryDiagnosticMemtesterResult::New();
    memtester_result->passed_items = {
        crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kSixteenBitWrites};
    memtester_result->failed_items = {
        crosapi::TelemetryDiagnosticMemtesterTestItemEnum::kEightBitWrites};

    auto memory_detail = crosapi::TelemetryDiagnosticMemoryRoutineDetail::New();
    memory_detail->bytes_tested = 42;
    memory_detail->result = std::move(memtester_result);

    auto finished_state =
        crosapi::TelemetryDiagnosticRoutineStateFinished::New();
    finished_state->detail =
        crosapi::TelemetryDiagnosticRoutineDetail::NewMemory(
            std::move(memory_detail));
    finished_state->has_passed = true;

    auto state = crosapi::TelemetryDiagnosticRoutineState::New();
    state->state_union =
        crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
            std::move(finished_state));

    control->SetState(std::move(state));
  }));

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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewUnmappedUnionField(0));
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
  auto exception = crosapi::TelemetryExtensionException::New();
  exception->debug_message = "TEST_MESSAGE";
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewException(
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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewSupported(
          crosapi::TelemetryExtensionSupported::New()));
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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewUnmappedUnionField(0));
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
  auto exception = crosapi::TelemetryExtensionException::New();
  exception->debug_message = "TEST_MESSAGE";
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewException(
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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewSupported(
          crosapi::TelemetryExtensionSupported::New()));
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
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kVolumeButton);
    ASSERT_TRUE(control);

    auto volume_button_detail =
        crosapi::TelemetryDiagnosticVolumeButtonRoutineDetail::New();

    auto finished_state =
        crosapi::TelemetryDiagnosticRoutineStateFinished::New();
    finished_state->detail =
        crosapi::TelemetryDiagnosticRoutineDetail::NewVolumeButton(
            std::move(volume_button_detail));
    finished_state->has_passed = true;

    auto state = crosapi::TelemetryDiagnosticRoutineState::New();
    state->state_union =
        crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
            std::move(finished_state));

    control->SetState(std::move(state));
  }));

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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewUnmappedUnionField(0));
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
  auto exception = crosapi::TelemetryExtensionException::New();
  exception->debug_message = "TEST_MESSAGE";
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewException(
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
  fake_service().SetIsRoutineArgumentSupportedResponse(
      crosapi::TelemetryExtensionSupportStatus::NewSupported(
          crosapi::TelemetryExtensionSupported::New()));
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
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kFan);
    ASSERT_TRUE(control);

    auto fan_detail = crosapi::TelemetryDiagnosticFanRoutineDetail::New();
    fan_detail->passed_fan_ids = {0};
    fan_detail->failed_fan_ids = {1};
    fan_detail->fan_count_status =
        crosapi::TelemetryDiagnosticHardwarePresenceStatus::kMatched;

    auto finished_state =
        crosapi::TelemetryDiagnosticRoutineStateFinished::New();
    finished_state->detail = crosapi::TelemetryDiagnosticRoutineDetail::NewFan(
        std::move(fan_detail));
    finished_state->has_passed = true;

    auto state = crosapi::TelemetryDiagnosticRoutineState::New();
    state->state_union =
        crosapi::TelemetryDiagnosticRoutineStateUnion::NewFinished(
            std::move(finished_state));

    control->SetState(std::move(state));
  }));

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
  base::test::TestFuture<crosapi::TelemetryDiagnosticRoutineInquiryReplyPtr>
      on_reply_to_inquiry;

  fake_service().SetOnCreateRoutineCalled(
      base::BindLambdaForTesting([this, &on_reply_to_inquiry]() {
        auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
            crosapi::TelemetryDiagnosticRoutineArgument::Tag::kMemory);
        ASSERT_TRUE(control);

        control->SetOnReplyToInquiryCalled(
            on_reply_to_inquiry.GetRepeatingCallback());
      }));

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

  auto reply = on_reply_to_inquiry.Take();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(reply->is_check_led_lit_up_state());
  EXPECT_EQ(reply->get_check_led_lit_up_state()->state,
            crosapi::TelemetryDiagnosticCheckLedLitUpStateReply::State::
                kCorrectColor);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateLedLitUpRoutineSuccess) {
  base::test::TestFuture<void> routine_created_future;
  fake_service().SetOnCreateRoutineCalled(
      routine_created_future.GetRepeatingCallback());

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

  EXPECT_TRUE(routine_created_future.Wait());
  EXPECT_TRUE(fake_service().GetCreatedRoutineControlForRoutineType(
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kLedLitUp));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateKeyboardBacklightRoutineSuccess) {
  base::test::TestFuture<void> routine_created_future;
  fake_service().SetOnCreateRoutineCalled(
      routine_created_future.GetRepeatingCallback());

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

  EXPECT_TRUE(routine_created_future.Wait());
  EXPECT_TRUE(fake_service().GetCreatedRoutineControlForRoutineType(
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kKeyboardBacklight));
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       ReplyToKeyboardBacklightRoutineInquirySuccess) {
  base::test::TestFuture<crosapi::TelemetryDiagnosticRoutineInquiryReplyPtr>
      on_reply_to_inquiry;

  fake_service().SetOnCreateRoutineCalled(
      base::BindLambdaForTesting([this, &on_reply_to_inquiry]() {
        auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
            crosapi::TelemetryDiagnosticRoutineArgument::Tag::
                kKeyboardBacklight);
        ASSERT_TRUE(control);

        control->SetOnReplyToInquiryCalled(
            on_reply_to_inquiry.GetRepeatingCallback());
      }));

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

  auto reply = on_reply_to_inquiry.Take();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(reply->is_check_keyboard_backlight_state());
  EXPECT_EQ(
      reply->get_check_keyboard_backlight_state()->state,
      crosapi::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State::kOk);
}

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticsApiV2BrowserTest,
                       CreateNetworkBandwidthRoutineSuccess) {
  fake_service().SetOnCreateRoutineCalled(base::BindLambdaForTesting([this]() {
    auto* control = fake_service().GetCreatedRoutineControlForRoutineType(
        crosapi::TelemetryDiagnosticRoutineArgument::Tag::kNetworkBandwidth);
    ASSERT_TRUE(control);

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

    control->SetState(std::move(state));
  }));

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
  base::test::TestFuture<void> routine_created_future;
  fake_service().SetOnCreateRoutineCalled(
      routine_created_future.GetRepeatingCallback());

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

  EXPECT_TRUE(routine_created_future.Wait());
  EXPECT_TRUE(fake_service().GetCreatedRoutineControlForRoutineType(
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kCameraFrameAnalysis));
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
