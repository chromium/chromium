// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"

#include <memory>
#include <string>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_browser_test.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_features.h"
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
  explicit EventRegistrationObserver(const std::string& event_name,
                                     base::OnceClosure on_event_added,
                                     content::BrowserContext* context)
      : context_(context),
        event_name_(event_name),
        on_event_added_(std::move(on_event_added)) {}
  ~EventRegistrationObserver() override = default;

  void OnListenerAdded(const extensions::EventListenerInfo& details) override {
    if (details.event_name.compare(event_name_)) {
      extensions::EventRouter::Get(context_)->UnregisterObserver(this);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_event_added_));
    }
  }

 private:
  base::raw_ptr<content::BrowserContext, ExperimentalAsh> context_;
  const std::string& event_name_;
  base::OnceClosure on_event_added_;
};

class TelemetryExtensionDiagnosticRoutineObserverBrowserTest
    : public BaseTelemetryExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseTelemetryExtensionBrowserTest::SetUpOnMainThread();

    observation_ = std::make_unique<DiagnosticRoutineObservation>(
        extension_id(), uuid_, profile(), remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  void RegisterEventObserver(const std::string& event_name,
                             base::OnceClosure on_event_added) {
    registration_observer_ = std::make_unique<EventRegistrationObserver>(
        event_name, std::move(on_event_added), profile());

    extensions::EventRouter::Get(profile())->RegisterObserver(
        registration_observer_.get(), event_name);
  }

  base::Uuid uuid_{base::Uuid::GenerateRandomV4()};
  mojo::Remote<crosapi::TelemetryDiagnosticRoutineObserver> remote_;

 private:
  std::unique_ptr<EventRegistrationObserver> registration_observer_;
  std::unique_ptr<DiagnosticRoutineObservation> observation_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TelemetryExtensionDiagnosticRoutineObserverBrowserTest,
                       CanObserveOnRoutineInitializedWithoutFeatureFlagFail) {
  CreateExtensionAndRunServiceWorker(R"(
    chrome.test.runTests([
      function canObserveOnRoutineInitializedFail() {
        chrome.test.assertThrows(() => {
          chrome.os.diagnostics.onRoutineInitialized.addListener((event) => {
            // unreachable
          });
        }, [],
          'Cannot read properties of undefined (reading \'addListener\')'
        );

        chrome.test.succeed();
      }
    ]);
  )");
}

class PendingApprovalTelemetryExtensionDiagnosticRoutineObserverBrowserTest
    : public TelemetryExtensionDiagnosticRoutineObserverBrowserTest {
 public:
  PendingApprovalTelemetryExtensionDiagnosticRoutineObserverBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kTelemetryExtensionPendingApprovalApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PendingApprovalTelemetryExtensionDiagnosticRoutineObserverBrowserTest,
    CanObserveOnRoutineInitialized) {
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

}  // namespace chromeos
