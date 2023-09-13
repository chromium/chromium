// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service_factory.h"
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

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionDiagnosticsApiV2BrowserTest,
    CreateMemoryRoutineWithFeatureFlagCompanionUiNotOpenError) {
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
                       CreateMemoryRoutineWithFeatureFlagResetConnection) {
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
                       FinishedRoutineIsRemovedWithFeatureFlagSuccess) {
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
                       CreateMemoryRoutineWithFeatureFlagSuccess) {
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
                       StartRoutineWithFeatureFlagUnknownUuidError) {
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
                       StartRoutineWithFeatureFlagSuccess) {
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
                       CancelRoutineWithFeatureFlagSuccess) {
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

IN_PROC_BROWSER_TEST_F(
    TelemetryExtensionDiagnosticsApiV2BrowserTest,
    IsMemoryRoutineArgSupportedWithFeatureFlagApiInternalError) {
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
                       IsMemoryRoutineArgSupportedWithFeatureFlagException) {
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
                       IsMemoryRoutineArgSupportedWithFeatureFlagSuccess) {
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

}  // namespace chromeos
