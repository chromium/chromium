// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/service_worker_host.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/service_worker/worker_id_set.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/service_worker_host.mojom-test-utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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
                        "<p>page</p>");
    ExtensionTestMessageListener extension_oninstall_listener_fired(
        "installed listener fired");
    const Extension* extension = LoadExtension(test_dir->UnpackedPath());
    test_extension_dirs_.push_back(std::move(test_dir));
    ASSERT_TRUE(extension);
    extension_ = extension;
    ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());

    // Verify the worker is running.
    ServiceWorkerTaskQueue::WorkerState* worker_state = GetWorkerState();
    ASSERT_TRUE(worker_state);
    const std::optional<WorkerId>& worker_id = worker_state->worker_id();
    ASSERT_TRUE(worker_id.has_value());
    ASSERT_TRUE(content::CheckServiceWorkerIsRunning(GetServiceWorkerContext(),
                                                     worker_id->version_id));
  }

  ServiceWorkerTaskQueue::WorkerState* GetWorkerState() {
    ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
    std::optional<base::UnguessableToken> activation_token =
        task_queue->GetCurrentActivationToken(extension_->id());
    if (!activation_token) {
      return nullptr;
    }
    ServiceWorkerTaskQueue::SequencedContextId context_id{
        extension_->id(), profile(), activation_token.value()};
    return task_queue->GetWorkerStateForTesting(context_id);
  }

  const Extension* extension() { return extension_; }

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
            ServiceWorkerTaskQueue::
                AllowMultipleWorkersPerExtensionForTesting()) {}

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
    NavigateInNewTab(extension_->GetResourceURL("extension_page_tab.html"));
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

  // Navigates the browser to a new tab at `url` and waits for it to load.
  void NavigateInNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WaitForLoadStop(web_contents);
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
// before ServiceWorkerTaskQueue::OnStopped().

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
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension()->id());
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

  // Remove the task queue as an observer of `ServiceWorkerContext` so that
  // the browser stop notification will not run immediately.
  ServiceWorkerTaskQueue::Get(profile())->StopObservingContextForTest(
      sw_context);

  TestServiceWorkerTaskQueueObserver worker_id_removed_observer;

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension()->id());
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
  ServiceWorkerTaskQueue::Get(profile())->OnStopped(
      previous_service_worker_id->version_id, sw_info);

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
      browser()->profile(), previous_service_worker_id->extension_id);
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
  task_queue->DidStopServiceWorkerContext(
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

// Test that if a browser stop notification is received before the render stop
// notification (since these things can be triggered independently) the worker's
// browser readiness remains not ready.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStopTrackingBrowserTest,
    OnStoppedUpdatesBrowserState_BeforeRenderStopNotification) {
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());

  // Get information about worker for extension that will be stopped soon.
  ServiceWorkerTaskQueue::WorkerState* worker_state = GetWorkerState();
  ASSERT_TRUE(worker_state);
  std::optional<WorkerId> stopped_service_worker_id = worker_state->worker_id();
  ASSERT_TRUE(stopped_service_worker_id);

  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  // Confirm the worker is browser state ready.
  std::optional<base::UnguessableToken> activation_token =
      task_queue->GetCurrentActivationToken(extension()->id());
  ASSERT_TRUE(activation_token);
  ASSERT_EQ(worker_state->browser_state(),
            ServiceWorkerTaskQueue::BrowserState::kReady);

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
      browser()->profile(), stopped_service_worker_id->extension_id);
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      GetServiceWorkerContext(), stopped_service_worker_id->version_id));

  // Confirm the worker state does still exist, and that the browser stop
  // notification reset it to no longer ready.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerTaskQueue::BrowserState::kInitial);

  // Simulate the render stop notification arriving afterwards.
  task_queue->DidStopServiceWorkerContext(
      stopped_service_worker_id->render_process_id,
      stopped_service_worker_id->extension_id, activation_token.value(),
      /*service_worker_scope=*/extension()->url(),
      stopped_service_worker_id->version_id,
      stopped_service_worker_id->thread_id);

  // Confirm the worker state still exists and browser state remains the same.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerTaskQueue::BrowserState::kInitial);
}

// Test that if a browser stop notification is received after the render stop
// notification (since these things can be triggered independently)
// it updates the worker's browser readiness information to not ready.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStopTrackingBrowserTest,
    OnStoppedUpdatesBrowserState_AfterRenderStopNotification) {
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());

  // Get information about worker for extension that will be stopped soon.
  ServiceWorkerTaskQueue::WorkerState* worker_state = GetWorkerState();
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
  ASSERT_EQ(worker_state->browser_state(),
            ServiceWorkerTaskQueue::BrowserState::kReady);

  // Remove the task queue as an observer of `ServiceWorkerContext` so that
  // the browser stop notification will not run immediately.
  ServiceWorkerTaskQueue::Get(profile())->StopObservingContextForTest(
      sw_context);

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension()->id());
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      sw_context, stopped_service_worker_id->version_id));

  // Confirm the worker state still exists and browser state is still ready.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerTaskQueue::BrowserState::kReady);

  // Simulate browser stop notification after the render stop notification.
  ServiceWorkerTaskQueue::Get(profile())->OnStopped(
      stopped_service_worker_id->version_id, sw_info);

  // Confirm the worker state still exists, but browser state is not ready.
  EXPECT_EQ(worker_state->browser_state(),
            ServiceWorkerTaskQueue::BrowserState::kInitial);
}

// Test that if a browser stop notification is received after a worker is
// deactivated (since they can be triggered independently) we don't update the
// worker's browser readiness information.
IN_PROC_BROWSER_TEST_F(ServiceWorkerStopTrackingBrowserTest,
                       OnStoppedRunsAfterDeactivatingWorker) {
  ASSERT_NO_FATAL_FAILURE(LoadServiceWorkerExtension());

  // Get information about worker for extension that will be deactivated soon.
  ServiceWorkerTaskQueue::WorkerState* worker_state = GetWorkerState();
  ASSERT_TRUE(worker_state);
  std::optional<WorkerId> deactivated_service_worker_id =
      worker_state->worker_id();
  ASSERT_TRUE(deactivated_service_worker_id);
  content::ServiceWorkerContext* sw_context =
      GetServiceWorkerContext(profile());
  ASSERT_TRUE(sw_context);
  ASSERT_TRUE(base::Contains(sw_context->GetRunningServiceWorkerInfos(),
                             deactivated_service_worker_id->version_id));
  const content::ServiceWorkerRunningInfo& sw_info =
      sw_context->GetRunningServiceWorkerInfos().at(
          deactivated_service_worker_id->version_id);

  // Confirm the worker is browser state ready.
  ASSERT_EQ(worker_state->browser_state(),
            ServiceWorkerTaskQueue::BrowserState::kReady);

  // Deactivate extension.
  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  task_queue->DeactivateExtension(extension());

  // Confirm the worker state does not exist.
  worker_state = GetWorkerState();
  ASSERT_FALSE(worker_state);

  // Simulate browser stop notification after deactivating the extension.
  ServiceWorkerTaskQueue::Get(profile())->OnStopped(
      deactivated_service_worker_id->version_id, sw_info);

  // Confirm the worker state still does not exist.
  EXPECT_FALSE(worker_state);
}

}  // namespace

}  // namespace extensions
