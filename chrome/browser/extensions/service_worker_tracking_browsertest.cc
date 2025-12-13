// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/sequenced_context_id.h"
#include "extensions/browser/service_worker/service_worker_host.h"
#include "extensions/browser/service_worker/service_worker_state.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/service_worker/worker_id_set.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/service_worker_host.mojom-test-utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

// Tests for validating the logic for keeping track of extension service
// workers. This is intentionally broad to include things like:
//   * Starting and stopping state of the service worker
//   * Keeping track of information for the running worker instance

namespace extensions {

namespace {

using service_worker_test_utils::TestServiceWorkerTaskQueueObserver;

// A helper class that intercepts the
// `ServiceWorkerHost::DidStopServiceWorkerContext()` mojom receiver method and
// does *not* forward the call onto the real `ServiceWorkerHost` implementation.
class ServiceWorkerHostInterceptorForWorkerStop
    : public mojom::ServiceWorkerHostInterceptorForTesting {
 public:
  // We use `worker_id` to have a weak handle to the `ServiceWorkerHost`
  // since the host can be destroyed due to the worker stop in the test (the
  // stop disconnects the mojom pipe and then destroys `ServiceWorkerHost`).
  // Using the preferred `mojo::test::ScopedSwapImplForTesting()` would attempt
  // to swap in a freed `ServiceWorkerHost*` when the test ends and cause a
  // crash.
  explicit ServiceWorkerHostInterceptorForWorkerStop(const WorkerId& worker_id)
      : worker_id_(worker_id) {
    auto* worker_host = extensions::ServiceWorkerHost::GetWorkerFor(worker_id_);
    CHECK(worker_host) << "There is no ServiceWorkerHost for WorkerId: "
                       << worker_id_ << " when creating the stop interceptor.";
    // Do not store a pointer `ServiceWorkerHost` to avoid lifetime issues,
    // we'll use the `worker_id` as a weak handle instead.
    std::ignore = worker_host->receiver_for_testing().SwapImplForTesting(this);
  }

  mojom::ServiceWorkerHost* GetForwardingInterface() override {
    // This should be non-null if this interface is still receiving events. This
    // causes all methods other than `DidStopServiceWorkerContext()` to be sent
    // along to the real implementation.
    auto* worker_host = extensions::ServiceWorkerHost::GetWorkerFor(worker_id_);
    CHECK(worker_host) << "There is no ServiceWorkerHost for WorkerId: "
                       << worker_id_
                       << " when attempting to forward a mojom call to the "
                          "real `ServiceWorkerHost` implementation.";
    return worker_host;
  }

 protected:
  // mojom::ServiceWorkerHost:
  void DidStopServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) override {
    // Do not call the real `ServiceWorkerHost::DidStopServiceWorkerContext()`
    // method to simulate that a stop notification was never sent from the
    // renderer worker thread.
  }

 private:
  const WorkerId worker_id_;
};

class ServiceWorkerTrackingBrowserTest : public ExtensionBrowserTest {
 public:
  ServiceWorkerTrackingBrowserTest() = default;

  ServiceWorkerTrackingBrowserTest(const ServiceWorkerTrackingBrowserTest&) =
      delete;
  ServiceWorkerTrackingBrowserTest& operator=(
      const ServiceWorkerTrackingBrowserTest&) = delete;

 protected:
  void TearDownOnMainThread() override {
    ExtensionBrowserTest::TearDownOnMainThread();
    extension_ = nullptr;
  }

  virtual std::string GetExtensionPageContent() const { return "<p>page</p>"; }

  void LoadServiceWorkerExtension() {
    // Load a basic extension with a service worker and wait for the worker to
    // start running.
    static constexpr char kManifest[] =
        R"({
            "name": "Test Extension",
            "manifest_version": 3,
            "version": "0.1",
            "background": {
              "service_worker" : "background.js"
            },
            "permissions": ["webNavigation"]
        })";
    // The extensions script listens for runtime.onInstalled (to detect install
    // and worker start completion) and webNavigation.onBeforeNavigate (to
    // realistically request worker start).
    static constexpr char kBackgroundScript[] =
        R"(
            chrome.runtime.onInstalled.addListener((details) => {
                chrome.test.sendMessage('installed listener fired');
            });
            chrome.webNavigation.onBeforeNavigate.addListener((details) => {
                chrome.test.sendMessage('listener fired');
            });
        )";
    auto test_dir = std::make_unique<TestExtensionDir>();
    test_dir->WriteManifest(kManifest);
    test_dir->WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);
    test_dir->WriteFile(FILE_PATH_LITERAL("extension_page_tab.html"),
                        GetExtensionPageContent());
    ExtensionTestMessageListener extension_oninstall_listener_fired(
        "installed listener fired");
    const Extension* extension = LoadExtension(
        test_dir->UnpackedPath(), {.wait_for_registration_stored = true});
    test_extension_dirs_.push_back(std::move(test_dir));
    ASSERT_TRUE(extension);
    extension_ = extension;
    ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

    // Verify the worker is running.
    ServiceWorkerState* worker_state = GetWorkerState();
    ASSERT_TRUE(worker_state);
    const std::optional<WorkerId>& worker_id = worker_state->worker_id();
    ASSERT_TRUE(worker_id.has_value());
    ASSERT_TRUE(content::CheckServiceWorkerIsRunning(GetServiceWorkerContext(),
                                                     worker_id->version_id));
  }

  ServiceWorkerState* GetWorkerState() {
    ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
    std::optional<base::UnguessableToken> activation_token =
        task_queue->GetCurrentActivationToken(extension_->id());
    if (!activation_token) {
      return nullptr;
    }
    SequencedContextId context_id{extension_->id(), profile()->UniqueId(),
                                  activation_token.value()};
    return task_queue->GetWorkerStateForTesting(context_id);
  }

  const Extension* extension() { return extension_; }

  TestExtensionDir* test_extension_dir() {
    if (test_extension_dirs_.size() != 1) {
      ADD_FAILURE() << "Expected exactly one test extension directory";
      return nullptr;
    }
    return test_extension_dirs_.front().get();
  }

  raw_ptr<const Extension> extension_;
  // Ensure `TestExtensionDir`s live past the test helper methods finishing.
  std::vector<std::unique_ptr<TestExtensionDir>> test_extension_dirs_;
};

// Test class to help verify the tracking of `WorkerId`s in
// `ServiceWorkerTaskQueue` and `WorkerIdSet`.
class ServiceWorkerIdTrackingBrowserTest
    : public ServiceWorkerTrackingBrowserTest {
 public:
  ServiceWorkerIdTrackingBrowserTest()
      // Prevent the test from hitting CHECKs so we can examine `WorkerIdSet` at
      // the end of the tests.
      : allow_multiple_worker_per_extension_in_worker_id_set_(
            WorkerIdSet::AllowMultipleWorkersPerExtensionForTesting()),
        allow_multiple_workers_per_extension_in_task_queue_(
            ServiceWorkerState::AllowMultipleWorkersPerExtensionForTesting()) {}

 protected:
  void SetUpOnMainThread() override {
    ServiceWorkerTrackingBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    process_manager_ = ProcessManager::Get(profile());
    ASSERT_TRUE(process_manager_);
  }

  void TearDownOnMainThread() override {
    ServiceWorkerTrackingBrowserTest::TearDownOnMainThread();
    process_manager_ = nullptr;
  }

  void OpenExtensionTab() {
    // Load a page from a resource inside the extension (and therefore inside
    // the extension render process). This prevents the //content layer from
    // completely shutting down the render process (which is another way that
    // eventually removes the worker from `WorkerIdSet`).
    SCOPED_TRACE("Loading extension tab for test extension");
    NavigateToURLInNewTab(
        extension_->GetResourceURL("extension_page_tab.html"));
  }

  void LoadServiceWorkerExtensionAndOpenExtensionTab() {
    LoadServiceWorkerExtension();
    OpenExtensionTab();
  }

  std::optional<WorkerId> GetWorkerIdForExtension() {
    std::vector<WorkerId> service_workers_for_extension =
        process_manager_->GetServiceWorkersForExtension(extension()->id());
    if (service_workers_for_extension.size() > 1u) {
      ADD_FAILURE() << "Expected only one worker for extension: "
                    << extension()->id()
                    << " But found incorrect number of workers: "
                    << service_workers_for_extension.size();
      return std::nullopt;
    }
    return service_workers_for_extension.empty()
               ? std::nullopt
               : std::optional<WorkerId>(service_workers_for_extension[0]);
  }

  // Starts the worker and waits for the worker to initialize.
  void StartWorker() {
    // Confirm the worker for the extension does not appear to be running,
    // otherwise this will hang forever.
    std::optional<WorkerId> worker_id = GetWorkerIdForExtension();
    ASSERT_EQ(std::nullopt, worker_id);

    // Add an observer to the task queue to detect when the new worker instance
    // `WorkerId` is added to `WorkerIdSet`.
    TestServiceWorkerTaskQueueObserver worker_id_added_observer;

    // Navigate somewhere to trigger the start of the worker to handle the
    // webNavigation.onBeforeRequest event.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("example.com", "/simple.html")));

    // Wait for the new worker instance to be added to `WorkerIdSet` (registered
    // in the process manager).
    SCOPED_TRACE(
        "Waiting for worker to restart in response to extensions event.");
    worker_id_added_observer.WaitForWorkerContextInitialized(extension()->id());
  }


 private:
  raw_ptr<ProcessManager> process_manager_;
  base::AutoReset<bool> allow_multiple_worker_per_extension_in_worker_id_set_;
  base::AutoReset<bool> allow_multiple_workers_per_extension_in_task_queue_;
};

// TODO(crbug.com/40936639): improve the stall test by using similar logic to
// ServiceWorkerVersionTest.StallInStopping_DetachThenStart to more closely
// simulate a worker thread delayed in stopping. This will also allow testing
// when the delay causes ProcessManager::RenderProcessExited() to be called
// before ServiceWorkerState::OnStoppedSync().

// Tests that when:
//   1) something, other than a worker, keeps the extension renderer process
//     alive (e.g. a tab is open to a page hosted inside the extension) and
//   2) simultaneously the worker is stopped but is stalled/blocked in
//     terminating (preventing notification to //extensions that it has stopped)
//     and
//   3) sometime later a new worker instance is started (e.g. by a new extension
//     event that is sent)
//
// (a.k.a a "delayed worker stop") the //extensions browser layer should only
// track (`WorkerIdSet`) one worker instance (`WorkerId`) (the new worker
// instance). This avoids tracking one or more instances of stopped workers.
// Regression test for crbug.com/40936639.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerIdTrackingBrowserTest,
    WorkerStalledInStopping_RemovedByBrowserStopNotification) {
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtensionAndOpenExtensionTab());

  // Get the soon to be stopped ("previous") worker's `WorkerId`.
  std::optional<WorkerId> previous_service_worker_id =
      GetWorkerIdForExtension();
  ASSERT_TRUE(previous_service_worker_id);

  // Setup intercept of `ServiceWorkerHost::DidStopServiceWorkerContext()` mojom
  // call. This simulates the worker renderer thread being very slow/never
  // informing the //extensions browser layer that the worker context/thread
  // terminated.
  ServiceWorkerHostInterceptorForWorkerStop stop_interceptor(
      *previous_service_worker_id);

  // Stop the service worker. Note: despite the worker actually terminating in
  // the test, `stop_interceptor` has intercepted and prevented the stop
  // notification from occurring which prevents the previous worker instance
  // from being removed from `WorkerIdSet`. Combined with the open extension tab
  // above the worker is simulated as being stalled/blocked in terminating.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension()->id());
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      GetServiceWorkerContext(), previous_service_worker_id->version_id));

  // Confirm after stopping we no longer have the previous `WorkerId`. The
  // browser stop notification should've removed it for us because the renderer
  // stop never happened.
  std::optional<WorkerId> worker_id_after_stop_worker =
      GetWorkerIdForExtension();
  ASSERT_EQ(std::nullopt, worker_id_after_stop_worker);

  // Start the new instance of the worker and wait for it to start.
  ASSERT_NO_FATAL_FAILURE(StartWorker());

  // Confirm that we are only tracking one running worker.
  std::optional<WorkerId> newly_started_service_worker_id =
      GetWorkerIdForExtension();
  ASSERT_TRUE(newly_started_service_worker_id);

  // Confirm `WorkerId` being tracked seems to be a different started instance
  // than the first one (WorkerIds are sorted by their attributes so the last is
  // considered the newest WorkerId since it has a higher thread, or process id,
  // etc.).
  // TODO(jlulejian): Is there a less fragile way of confirming this? If the
  // same render process uses the same thread ID this would then fail.
  EXPECT_NE(newly_started_service_worker_id, previous_service_worker_id);
}

// Test that when a worker is stopped and then restarted we only track one
// instance of `WorkerId` in `WorkerIdSet`. This specific test removes it via
// the renderer stop notification first (but it could also happen in other ways)
// and then ensures the browser stop notification doesn't try to doubly remove
// the `WorkerId`.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerIdTrackingBrowserTest,
    WorkerNotStalledInStopping_RemovedByRenderStopNotificationFirst) {
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtensionAndOpenExtensionTab());

  // Get the soon to be stopped ("previous") worker's information.
  std::optional<WorkerId> previous_service_worker_id =
      GetWorkerIdForExtension();
  ASSERT_TRUE(previous_service_worker_id);
  content::ServiceWorkerContext* sw_context =
      GetServiceWorkerContext(profile());
  ASSERT_TRUE(sw_context);
  ASSERT_TRUE(base::Contains(sw_context->GetRunningServiceWorkerInfos(),
                             previous_service_worker_id->version_id));
  const content::ServiceWorkerRunningInfo& sw_info =
      sw_context->GetRunningServiceWorkerInfos().at(
          previous_service_worker_id->version_id);
  // Remove the worker state as an observer of `ServiceWorkerContext` so that
  // the browser stop notification will not run immediately.
  ServiceWorkerState* worker_state = GetWorkerState();
  worker_state->StopObservingContextForTest();

  TestServiceWorkerTaskQueueObserver worker_id_removed_observer;

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension()->id());
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      sw_context, previous_service_worker_id->version_id));

  worker_id_removed_observer.WaitForWorkerStopped(extension()->id());

  // Confirm after stopping we no longer have the previous `WorkerId` (it was
  // removed by the renderer stop notification).
  std::optional<WorkerId> worker_id_after_stop_worker_renderer =
      GetWorkerIdForExtension();
  ASSERT_EQ(std::nullopt, worker_id_after_stop_worker_renderer);

  // Run the browser stop notification after the renderer stop notification, and
  // it should do nothing.
  worker_state->OnStoppedSync(previous_service_worker_id->version_id,
                              sw_info.scope);

  // Confirm after the browser stop notification that we are still no longer
  // tracking the worker.
  std::optional<WorkerId> worker_id_after_stop_worker_browser =
      GetWorkerIdForExtension();
  ASSERT_EQ(std::nullopt, worker_id_after_stop_worker_browser);
}

// Test that when a worker is stopped and then restarted we only track one
// instance of `WorkerId` in `WorkerIdSet`. This specific test removes it via
// the browser stop notification first and then ensures the renderer stop
// notification doesn't try to doubly remove the `WorkerId`.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerIdTrackingBrowserTest,
    WorkerNotStalledInStopping_RemovedByBrowserStopNotificationFirst) {
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtensionAndOpenExtensionTab());

  // Get the soon to be stopped ("previous") worker's `WorkerId`.
  std::optional<WorkerId> previous_service_worker_id =
      GetWorkerIdForExtension();
  ASSERT_TRUE(previous_service_worker_id);

  // Get the activation token for later passing to the render stop notification.
  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  auto activation_token =
      task_queue->GetCurrentActivationToken(extension()->id());
  ASSERT_TRUE(activation_token);

  // Setup intercept of `ServiceWorkerHost::DidStopServiceWorkerContext()` mojom
  // call. This simulates the worker renderer thread being very slow/never
  // informing the //extensions browser layer that the worker context/thread
  // terminated.
  ServiceWorkerHostInterceptorForWorkerStop stop_interceptor(
      *previous_service_worker_id);

  // Stop the service worker. Note: despite the worker actually terminating in
  // the test, `stop_interceptor` has intercepted and prevented the stop
  // notification from occurring which prevents the previous worker instance
  // from being removed from `WorkerIdSet`. Combined with the open extension tab
  // above the worker is simulated as being stalled/blocked in terminating.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      profile(), previous_service_worker_id->extension_id);
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      GetServiceWorkerContext(), previous_service_worker_id->version_id));

  // Confirm after stopping we no longer have the previous `WorkerId`. The
  // browser stop notification should've removed it for us.
  std::optional<WorkerId> worker_id_after_stop_worker_browser =
      GetWorkerIdForExtension();
  ASSERT_EQ(std::nullopt, worker_id_after_stop_worker_browser);

  // TODO(crbug.com/40936639): test this with `ServiceWorkerHost` rather than
  // `ServiceWorkerTaskQueue` once we can mimic the stalling situation
  // precisely. As-is these tests actually stop the render which destroys
  // `ServiceWorkerHost`.
  // "Send" the render stop notification second
  task_queue->RendererDidStopServiceWorkerContext(
      previous_service_worker_id->render_process_id,
      previous_service_worker_id->extension_id, activation_token.value(),
      /*service_worker_scope=*/extension()->url(),
      previous_service_worker_id->version_id,
      previous_service_worker_id->thread_id);

  // Confirm after the renderer stop notification we still no longer have the
  // previous `WorkerId`.
  std::optional<WorkerId> worker_id_after_stop_worker_renderer =
      GetWorkerIdForExtension();
  ASSERT_EQ(std::nullopt, worker_id_after_stop_worker_renderer);
}

using ServiceWorkerStopTrackingBrowserTest = ServiceWorkerTrackingBrowserTest;

class ServiceWorkerStopTrackingBrowserTestWithOptimizeServiceWorkerStart
    : public ServiceWorkerStopTrackingBrowserTest,
      public base::test::WithFeatureOverride {
 public:
  ServiceWorkerStopTrackingBrowserTestWithOptimizeServiceWorkerStart()
      : WithFeatureOverride(
            extensions_features::kOptimizeServiceWorkerStartRequests) {}
};

// Test that if a browser stop notification is received before the render stop
// notification (since these things can be triggered independently) the worker's
// browser and renderer state are both set to not ready.
IN_PROC_BROWSER_TEST_P(
    ServiceWorkerStopTrackingBrowserTestWithOptimizeServiceWorkerStart,
    OnStoppedUpdatesBrowserAndRendererState_BeforeRenderStopNotification) {
  const bool wakeup_optimization_enabled = IsParamFeatureEnabled();
  const auto kExpectedBrowserState =
      wakeup_optimization_enabled ? ServiceWorkerState::BrowserState::kActive
                                  : ServiceWorkerState::BrowserState::kReady;

  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());

  // Get information about worker for extension that will be stopped soon.
  ServiceWorkerState* worker_state = GetWorkerState();
  ASSERT_TRUE(worker_state);
  std::optional<WorkerId> stopped_service_worker_id = worker_state->worker_id();
  ASSERT_TRUE(stopped_service_worker_id);

  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  // Confirm the worker is browser state ready.
  std::optional<base::UnguessableToken> activation_token =
      task_queue->GetCurrentActivationToken(extension()->id());
  ASSERT_TRUE(activation_token);
  ASSERT_EQ(worker_state->browser_state(), kExpectedBrowserState);

  // Setup intercept of `ServiceWorkerHost::DidStopServiceWorkerContext()` mojom
  // call. This simulates the worker renderer thread being very slow/never
  // informing the //extensions browser layer that the worker context/thread
  // terminated.
  ServiceWorkerHostInterceptorForWorkerStop stop_interceptor(
      *stopped_service_worker_id);

  // Stop the service worker. Note: despite the worker actually terminating in
  // the test, `stop_interceptor` has intercepted and prevented the render stop
  // notification from occurring.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      profile(), stopped_service_worker_id->extension_id);
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      GetServiceWorkerContext(), stopped_service_worker_id->version_id));

  // Confirm the worker state does still exist, and that the browser stop
  // notification reset it to no longer ready.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerState::BrowserState::kNotActive);
  EXPECT_EQ(worker_state->renderer_state(),
            ServiceWorkerState::RendererState::kNotActive);

  // Confirm the worker has been untracked from ProcessManager.
  std::vector<WorkerId> workers_for_extension =
      ProcessManager::Get(profile())->GetServiceWorkersForExtension(
          extension()->id());
  EXPECT_EQ(workers_for_extension.size(), 0ul);

  // Simulate the render stop notification arriving afterwards.
  task_queue->RendererDidStopServiceWorkerContext(
      stopped_service_worker_id->render_process_id,
      stopped_service_worker_id->extension_id, activation_token.value(),
      /*service_worker_scope=*/extension()->url(),
      stopped_service_worker_id->version_id,
      stopped_service_worker_id->thread_id);

  // Confirm the worker state still exists and state remains the same.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerState::BrowserState::kNotActive);
  EXPECT_EQ(worker_state->renderer_state(),
            ServiceWorkerState::RendererState::kNotActive);
}

// Test that if a browser stop notification is received after the render stop
// notification (since these things can be triggered independently)
// the worker's browser and renderer readiness information remains not ready.
IN_PROC_BROWSER_TEST_P(
    ServiceWorkerStopTrackingBrowserTestWithOptimizeServiceWorkerStart,
    OnStoppedUpdatesBrowserAndRendererState_AfterRenderStopNotification) {
  const bool wakeup_optimization_enabled = IsParamFeatureEnabled();
  const auto kExpectedBrowserState =
      wakeup_optimization_enabled ? ServiceWorkerState::BrowserState::kActive
                                  : ServiceWorkerState::BrowserState::kReady;

  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());

  // Get information about worker for extension that will be stopped soon.
  ServiceWorkerState* worker_state = GetWorkerState();
  ASSERT_TRUE(worker_state);
  std::optional<WorkerId> stopped_service_worker_id = worker_state->worker_id();
  ASSERT_TRUE(stopped_service_worker_id);
  content::ServiceWorkerContext* sw_context =
      GetServiceWorkerContext(profile());
  ASSERT_TRUE(sw_context);
  ASSERT_TRUE(base::Contains(sw_context->GetRunningServiceWorkerInfos(),
                             stopped_service_worker_id->version_id));
  const content::ServiceWorkerRunningInfo& sw_info =
      sw_context->GetRunningServiceWorkerInfos().at(
          stopped_service_worker_id->version_id);

  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  // Confirm the worker is browser state ready.
  ASSERT_EQ(worker_state->browser_state(), kExpectedBrowserState);

  // Remove the worker state as an observer of `ServiceWorkerContext` so that
  // the browser stop notification will not run immediately.
  worker_state->StopObservingContextForTest();

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension()->id());
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      sw_context, stopped_service_worker_id->version_id));

  // Confirm the worker state still exists and browser and renderer state are
  // not ready.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerState::BrowserState::kNotActive);
  EXPECT_EQ(worker_state->renderer_state(),
            ServiceWorkerState::RendererState::kNotActive);

  // Simulate browser stop notification after the render stop notification.
  worker_state->OnStoppedSync(stopped_service_worker_id->version_id,
                              sw_info.scope);

  // Confirm the worker state still exists, and browser and renderer state
  // remain not ready.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerState::BrowserState::kNotActive);
  EXPECT_EQ(worker_state->renderer_state(),
            ServiceWorkerState::RendererState::kNotActive);

  // Confirm the worker has been untracked from ProcessManager.
  std::vector<WorkerId> workers_for_extension =
      ProcessManager::Get(profile())->GetServiceWorkersForExtension(
          extension()->id());
  EXPECT_EQ(workers_for_extension.size(), 0ul);
}

// Test that if an extension and its worker are deactivated, the worker is
// untracked from both ServiceWorkerTaskQueue and ProcessManager.
IN_PROC_BROWSER_TEST_P(
    ServiceWorkerStopTrackingBrowserTestWithOptimizeServiceWorkerStart,
    DisablingExtensionUntracksWorker) {
  const bool wakeup_optimization_enabled = IsParamFeatureEnabled();
  const auto kExpectedBrowserState =
      wakeup_optimization_enabled ? ServiceWorkerState::BrowserState::kActive
                                  : ServiceWorkerState::BrowserState::kReady;

  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());

  // Get information about worker for extension that will be deactivated soon.
  ServiceWorkerState* worker_state = GetWorkerState();
  ASSERT_TRUE(worker_state);
  std::optional<WorkerId> deactivated_service_worker_id =
      worker_state->worker_id();
  ASSERT_TRUE(deactivated_service_worker_id);
  content::ServiceWorkerContext* sw_context =
      GetServiceWorkerContext(profile());
  ASSERT_TRUE(sw_context);
  ASSERT_TRUE(base::Contains(sw_context->GetRunningServiceWorkerInfos(),
                             deactivated_service_worker_id->version_id));

  // Confirm the worker is browser state ready.
  ASSERT_EQ(worker_state->browser_state(), kExpectedBrowserState);

  // Deactivate extension.
  extensions::ExtensionRegistrar::Get(profile())->DisableExtension(
      extension()->id(), {disable_reason::DISABLE_USER_ACTION});

  // Confirm the worker state does not exist.
  worker_state = GetWorkerState();
  ASSERT_FALSE(worker_state);

  // Confirm the worker has been untracked from ProcessManager.
  std::vector<WorkerId> workers_for_extension =
      ProcessManager::Get(profile())->GetServiceWorkersForExtension(
          extension()->id());
  EXPECT_EQ(workers_for_extension.size(), 0ul);
}

// Toggle `extensions_features::OptimizeServiceWorkerStartRequests`.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ServiceWorkerStopTrackingBrowserTestWithOptimizeServiceWorkerStart);

// Test that if a renderer process exit notification is received before
// a browser stop notification (since these things can be triggered
// independently) and a context stop notification, it updates the worker's
// browser and renderer active state to inactive.
IN_PROC_BROWSER_TEST_F(ServiceWorkerStopTrackingBrowserTest,
                       RenderProcessExitedUpdatesBrowserAndRendererState) {
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());

  // Get information about worker for extension that will be stopped soon.
  ServiceWorkerState* worker_state = GetWorkerState();
  ASSERT_TRUE(worker_state);
  std::optional<WorkerId> worker_id = worker_state->worker_id();
  ASSERT_TRUE(worker_id);
  content::ServiceWorkerContext* sw_context =
      GetServiceWorkerContext(profile());
  ASSERT_TRUE(sw_context);
  ASSERT_TRUE(base::Contains(sw_context->GetRunningServiceWorkerInfos(),
                             worker_id->version_id));

  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  // Confirm the worker is renderer state active.
  ASSERT_EQ(worker_state->renderer_state(),
            ServiceWorkerState::RendererState::kActive);

  // Remove the worker state as an observer of `ServiceWorkerContext` so that
  // the browser stop notification will not run immediately.
  worker_state->StopObservingContextForTest();
  // Setup intercept of `ServiceWorkerHost::DidStopServiceWorkerContext()`.
  // This simulates the worker renderer thread never informing that the worker
  // context terminated.
  ServiceWorkerHostInterceptorForWorkerStop stop_interceptor(*worker_id);

  // Kill the service worker's renderer.
  content::RenderProcessHost* worker_render_process_host =
      content::RenderProcessHost::FromID(worker_id->render_process_id);
  ASSERT_TRUE(worker_render_process_host);
  content::RenderProcessHostWatcher process_exit_observer(
      worker_render_process_host,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  worker_render_process_host->Shutdown(content::RESULT_CODE_KILLED);
  process_exit_observer.Wait();

  // Verify the service worker was stopped.
  ASSERT_TRUE(
      content::CheckServiceWorkerIsStopped(sw_context, worker_id->version_id));

  // Confirm the worker state still exists and browser and renderer states have
  // been set to inactive by `ServiceWorkerHost::RenderProcessForWorkerExited`.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerState::BrowserState::kNotActive);
  EXPECT_EQ(worker_state->renderer_state(),
            ServiceWorkerState::RendererState::kNotActive);
}

using ServiceWorkerRendererTrackingBrowserTest = ExtensionApiTest;

// Tests that when reloading an extension that has a worker to a version of the
// extension that doesn't have a worker, we don't persist the worker activation
// token in the renderer across extension loads/unloads.
// Regression test for crbug.com/372753069.
// TODO(crbug.com/372753069): Duplicate this test for extension updates.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRendererTrackingBrowserTest,
                       UnloadingExtensionClearsRendererActivationToken) {
  embedded_test_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Initial version has a worker.
  static constexpr char kManifestWithWorker[] =
      R"({
           "name": "Test extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kWorkerBackground[] =
      R"(chrome.test.sendMessage('ready');)";
  // New version no longer has a worker (it adds a content script only for test
  // waiting purposes).
  static constexpr char kManifestWithoutWorker[] =
      R"({
           "name": "Test extension",
           "manifest_version": 3,
           "version": "0.2",
           "content_scripts": [{
             "matches": ["<all_urls>"],
             "js": ["script.js"],
             "all_frames": true,
             "run_at": "document_start"
           }]
         })";
  static constexpr char kContentScript[] =
      R"(chrome.test.sendMessage('script injected');)";
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifestWithWorker);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                          kWorkerBackground);
  extension_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kContentScript);

  // Install initial version of the extension with a worker.
  const Extension* extension = nullptr;
  ExtensionTestMessageListener listener("ready");
  {
    // This installation will populate a worker activation token in the
    // extension renderer for the worker.
    SCOPED_TRACE("installing extension with a worker");
    extension = LoadExtension(extension_dir.UnpackedPath());
    ASSERT_TRUE(extension);
  }

  // By waiting for the worker to be started and receive an event, we indirectly
  // can be fairly certain that the renderer has loaded the extension and
  // populated the worker activation token.
  {
    SCOPED_TRACE("waiting extension with worker's background script to start");
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  const ExtensionId original_extension_id = extension->id();

  // Reload to the new version of the extension without a worker.
  extension_dir.WriteManifest(kManifestWithoutWorker);

  ExtensionTestMessageListener extension_without_worker_loaded(
      "script injected");

  // Reload the extension so it no longer has a worker.
  {
    SCOPED_TRACE(
        "reloading extension with a worker to new version without a worker");
    // Reloading the extension should unload the original version of the
    // extension which will remove the worker activation token in the renderer.
    // Then the subsequent load will load the new version of the extension
    // without a worker activation token. The bug was that the token was never
    // removed and would remain across this reload. We CHECK() that non-worker
    // based extension do not have activation tokens which would've crash the
    // renderer prior to the fix.
    ReloadExtension(original_extension_id);
  }

  // To indirectly confirm that the extension without a worker loaded in the
  // renderer we navigate to a page and wait for the content script to run.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));
  {
    SCOPED_TRACE("waiting for extension without worker to load");
    ASSERT_TRUE(extension_without_worker_loaded.WaitUntilSatisfied());
  }

  // Confirm the extension updated to the new version without a worker.
  const Extension* new_extension_version =
      ExtensionRegistry::Get(profile())->GetInstalledExtension(
          original_extension_id);
  ASSERT_TRUE(new_extension_version);
  ASSERT_EQ("0.2", new_extension_version->version().GetString());

  // Double-confirm that after our wait the renderer hasn't crashed.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_FALSE(web_contents->IsCrashed());
}

// Tests tracking behavior of the main extension service worker when an
// additional service worker is registered by the extension for a sub-scope
// via `navigator.serviceWorker.register()` from an extension page.
class
    ServiceWorkerSubScopeWorkerTrackingBrowserTestWithOptimizeServiceWorkerStart
    : public ServiceWorkerIdTrackingBrowserTest,
      public base::test::WithFeatureOverride {
 public:
  ServiceWorkerSubScopeWorkerTrackingBrowserTestWithOptimizeServiceWorkerStart()
      : WithFeatureOverride(
            extensions_features::kOptimizeServiceWorkerStartRequests) {}

 protected:
  std::string GetExtensionPageContent() const override {
    return R"(<script src="/page.js"></script>)";
  }

  void LoadSubScopeServiceWorker() {
    // Code for a service worker that will be registered for a sub-scope
    // of the extension root scope. This service worker is not allowed
    // access to extension APIs, as it's not listed in the manifest.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::CreateDirectory(test_extension_dir()->UnpackedPath().Append(
          FILE_PATH_LITERAL("subscope")));
      test_extension_dir()->WriteFile(FILE_PATH_LITERAL("subscope/sw.js"), R"(
          console.log("subscope service worker");
      )");
    }

    // Code for the script that will be executed as part of the extension page.
    // This registers the previously defined service worker.
    test_extension_dir()->WriteFile(FILE_PATH_LITERAL("page.js"), R"(
        navigator.serviceWorker.register("subscope/sw.js").then(function() {
          // Wait until the service worker is active.
          return navigator.serviceWorker.ready;
        }).catch(function(err) {
          console.log("registration error: " + err.message);
        });
    )");

    // Open the extension page, which will cause the sub-scope service
    // worker to start. We wait for its registration here.
    content::ServiceWorkerContext* sw_context =
        GetServiceWorkerContext(profile());
    service_worker_test_utils::TestServiceWorkerContextObserver
        registration_observer(sw_context);
    OpenExtensionTab();
    registration_observer.WaitForRegistrationStored();
  }
};

// Tests that stopping a service worker that was registered for
// a sub-scope via `navigation.serviceWorker.register()`, rather
// than being declared in the extension's manifest does not influence the
// tracking of the main extension service worker. Regression test for
// crbug.com/395536907.
IN_PROC_BROWSER_TEST_P(
    ServiceWorkerSubScopeWorkerTrackingBrowserTestWithOptimizeServiceWorkerStart,
    StoppingSubScopeWorkerDoesNotAffectExtensionWorker) {
  const bool wakeup_optimization_enabled = IsParamFeatureEnabled();
  const auto kExpectedBrowserState =
      wakeup_optimization_enabled ? ServiceWorkerState::BrowserState::kActive
                                  : ServiceWorkerState::BrowserState::kReady;

  // Load the extension service worker. This method will wait for its
  // registration to be stored and the service worker to be running.
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());
  // Load the sub-scope service worker and open the extension tab.
  // This method will wait for the registration to be stored.
  ASSERT_NO_FATAL_FAILURE(LoadSubScopeServiceWorker());

  // Confirm that we are tracking the main extension service worker.
  std::optional<WorkerId> extension_service_worker_id =
      GetWorkerIdForExtension();
  ASSERT_TRUE(extension_service_worker_id);

  // Check that there's 2 service workers running in total.
  content::ServiceWorkerContext* sw_context =
      GetServiceWorkerContext(profile());
  ASSERT_TRUE(sw_context);
  EXPECT_EQ(sw_context->GetRunningServiceWorkerInfos().size(), 2ul);
  // One of them should be the main extension service worker.
  EXPECT_TRUE(sw_context->GetRunningServiceWorkerInfos().contains(
      extension_service_worker_id->version_id));

  // Stop the sub-scope service worker.
  TestServiceWorkerTaskQueueObserver untracked_observer;
  GURL sub_scope(extension()->url().spec() + "subscope/");
  content::StopServiceWorkerForScope(sw_context, sub_scope, base::DoNothing());
  // Wait until the code responsible for untracking workers is called.
  untracked_observer.WaitForUntrackServiceWorkerState(sub_scope);

  // Verify that the main extension service worker is still tracked as running
  // by the task queue.
  ServiceWorkerState* worker_state = GetWorkerState();
  EXPECT_EQ(worker_state->browser_state(), kExpectedBrowserState);
  EXPECT_EQ(worker_state->renderer_state(),
            ServiceWorkerState::RendererState::kActive);
  EXPECT_TRUE(worker_state->worker_id());
}

// Toggle `extensions_features::OptimizeServiceWorkerStartRequests`.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ServiceWorkerSubScopeWorkerTrackingBrowserTestWithOptimizeServiceWorkerStart);

}  // namespace

}  // namespace extensions
