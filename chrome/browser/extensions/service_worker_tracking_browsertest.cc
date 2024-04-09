// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>

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

// Tests for extension service worker behavior outside of worker API or event
// dispatching logic.

namespace extensions {

namespace {

// A helper class that intercepts the
// `ServiceWorkerHost::DidStopServiceWorkerContext()` mojom receiver method,
// provides some of the call's arguments to an optional observer, and does *not*
// forward the call onto the real `ServiceWorkerHost` implementation.
class ServiceWorkerHostInterceptorForWorkerStop
    : public mojom::ServiceWorkerHostInterceptorForTesting {
 public:
  // We use `worker_id` to have a weak handle to the `ServiceWorkerHost`
  // which can be destroyed due to the worker stop request sent during the test
  // (the stop disconnects the mojom pipe and then destroys
  // `ServiceWorkerHost`). Using the preferred
  // `mojo::test::ScopedSwapImplForTesting()` would attempt to swap in a freed
  // `ServiceWorkerHost*` when the test ends and cause a crash.
  explicit ServiceWorkerHostInterceptorForWorkerStop(const WorkerId& worker_id)
      : worker_id_(worker_id) {
    auto* worker_host = extensions::ServiceWorkerHost::GetWorkerFor(worker_id_);
    CHECK(worker_host) << "There is no ServiceWorkerHost when for WorkerId: "
                       << worker_id_ << " when creating the stop interceptor.";
    // Do not store a pointer `ServiceWorkerHost` to avoid lifetime issues,
    // we'll use the `worker_id` as a weak handle instead.
    std::ignore = worker_host->receiver_for_testing().SwapImplForTesting(this);
  }

  mojom::ServiceWorkerHost* GetForwardingInterface() override {
    // This should be non-null if this interface is still receiving events.
    auto* worker_host = extensions::ServiceWorkerHost::GetWorkerFor(worker_id_);
    CHECK(worker_host) << "There is no ServiceWorkerHost for WorkerId: "
                       << worker_id_
                       << " when attempting to forward a mojom call to the "
                          "real `ServiceWorkerHost` implemenation.";
    return worker_host;
  }

  using DidStopServiceWorkerContextObserver =
      base::RepeatingCallback<void(const ExtensionId& extension_id,
                                   int64_t service_worker_version_id)>;
  void SetDidStopServiceWorkerContextObserver(
      DidStopServiceWorkerContextObserver did_stop_worker_observer) {
    did_stop_worker_observer_ = std::move(did_stop_worker_observer);
  }

 protected:
  // mojom::ServiceWorkerHost:
  void DidStopServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) override {
    if (did_stop_worker_observer_) {
      did_stop_worker_observer_.Run(extension_id, service_worker_version_id);
    }
    // Do not call the real `ServiceWorkerHost::DidStopServiceWorkerContext()`
    // method to simulate that a stop notification was never sent from the
    // renderer worker thread.
  }

 private:
  DidStopServiceWorkerContextObserver did_stop_worker_observer_;
  const WorkerId worker_id_;
};

// A helper class to wait for a service worker for an extension with
// `extension_id` to be initialized (and indirectly know that the new worker
// should've been added to `WorkerIdSet`).
class WorkerInitWaiter : public ServiceWorkerTaskQueue::TestObserver {
 public:
  explicit WorkerInitWaiter(const ExtensionId& extension_id)
      : extension_id_(extension_id) {
    ServiceWorkerTaskQueue::SetObserverForTest(this);
  }

  ~WorkerInitWaiter() override {
    ServiceWorkerTaskQueue::SetObserverForTest(nullptr);
  }

  void WaitForInit() { worker_inited_run_loop.Run(); }

 private:
  // ServiceWorkerTaskQueue::TestObserver:
  void DidInitializeServiceWorkerContext(
      const ExtensionId& extension_id) override {
    if (extension_id == extension_id_) {
      worker_inited_run_loop.Quit();
    }
  }

  const std::string extension_id_;
  base::RunLoop worker_inited_run_loop;
};

class ServiceWorkerTrackingBrowserTest : public ExtensionBrowserTest {
 public:
  ServiceWorkerTrackingBrowserTest()
      // Prevent the test from hitting CHECKs so we can examine `WorkerIdSet` at
      // the end of the test.
      : allow_multiple_worker_per_extension_in_worker_id_set_(
            WorkerIdSet::AllowMultipleWorkersPerExtensionForTesting()),
        allow_multiple_workers_per_extension_in_task_queue_(
            ServiceWorkerTaskQueue::
                AllowMultipleWorkersPerExtensionForTesting()) {}

  ServiceWorkerTrackingBrowserTest(const ServiceWorkerTrackingBrowserTest&) =
      delete;
  ServiceWorkerTrackingBrowserTest& operator=(
      const ServiceWorkerTrackingBrowserTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
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

 private:
  base::AutoReset<bool> allow_multiple_worker_per_extension_in_worker_id_set_;
  base::AutoReset<bool> allow_multiple_workers_per_extension_in_task_queue_;
};

// TODO(crbug.com/40936639): improve this test by using similar logic to
// ServiceWorkerVersionTest.StallInStopping_DetachThenStart to more closely
// simulate a worker thread delayed in stopping.

// Tests that when:
//   1) something, other than a worker, keeps the extension renderer process
//     alive (e.g. a tab is open to a page hosted inside the extension) and
//   2) simultaneously the worker is stopped but is stalled/blocked in
//     terminating (preventing notification to //extensions that it has stopped)
//     and
//   3) sometime later a new worker instance is started (e.g. by a new extension
//     event that is sent)
//
// the //extensions browser layer should only track one worker instance (the new
// worker instance). This avoids tracking multiple shutdown instances of the
// worker. Regression test for crbug.com/40936639.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTrackingBrowserTest,
                       WorkerStalledInStopping) {
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
      R"({
        chrome.runtime.onInstalled.addListener((details) => {
          chrome.test.sendMessage('installed listener fired');
        });
        chrome.webNavigation.onBeforeNavigate.addListener((details) => {
          chrome.test.sendMessage('listener fired');
        });
      })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);
  test_dir.WriteFile(FILE_PATH_LITERAL("extension_page_tab.html"),
                     "<p>page</p>");
  ExtensionTestMessageListener extension_oninstall_listener_fired(
      "installed listener fired");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_oninstall_listener_fired.WaitUntilSatisfied());
  // First SW version ID is always 0 and remains consistent until the extension
  // is deactivated.
  ASSERT_TRUE(content::CheckServiceWorkerIsRunning(
      GetServiceWorkerContext(), /*service_worker_version_id=*/0));

  // Load a page from a resource inside the extension (and therefore inside the
  // extension render process). This prevents the //content layer from
  // completely shutting down the render process (which is another way that
  // eventually removes the worker from `WorkerIdSet`).
  NavigateInNewTab(extension->GetResourceURL("extension_page_tab.html"));

  // Setup intercept of `ServiceWorkerHost::DidStopServiceWorkerContext()`
  // mojom call. This simulates the worker thread being very slow/never
  // informing the //extensions browser layer that the worker context/thread
  // terminated.
  std::vector<WorkerId> service_workers_for_extension =
      ProcessManager::Get(browser()->profile())
          ->GetServiceWorkersForExtension(extension->id());
  ASSERT_EQ(service_workers_for_extension.size(), 1u);
  const WorkerId& previous_service_worker_id = service_workers_for_extension[0];
  ServiceWorkerHostInterceptorForWorkerStop stop_interceptor(
      previous_service_worker_id);
  stop_interceptor.SetDidStopServiceWorkerContextObserver(
      base::BindLambdaForTesting([&](const ExtensionId& extension_id,
                                     int64_t service_worker_version_id) {
        ASSERT_EQ(extension->id(), extension_id);
        ASSERT_EQ(service_worker_version_id, 0);
      }));

  // Stop the service worker. Note: despite the worker actually terminating in
  // the test, `stop_interceptor` has intercepted and prevented the stop
  // notification from occurring which prevents the previous worker instance
  // from being removed from `WorkerIdSet`. Combined with the open extension tab
  // above the worker is simulated as being stalled/blocked in terminating.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension->id());
  ASSERT_TRUE(content::CheckServiceWorkerIsStopped(
      GetServiceWorkerContext(), /*service_worker_version_id=*/0));

  // Confirm after stopping we no longer have the previous `WorkerId` registered
  // in the ProcessManager.
  ProcessManager* process_manager = ProcessManager::Get(profile());
  ASSERT_TRUE(process_manager);
  std::vector<WorkerId> service_workers_after_stop_worker =
      process_manager->GetServiceWorkersForExtension(extension->id());
  // TODO(crbug.com/40936639): Once this bug is fixed, enable this assert.
  // ASSERT_TRUE(service_workers_after_stop_worker.empty());
  ASSERT_EQ(service_workers_after_stop_worker.size(), 1u);

  // Add an observer to the task queue to detect when the new worker instance
  // `WorkerId` is added to `WorkerIdSet` (registered in the process manager).
  WorkerInitWaiter worker_id_added_observer(extension->id());

  // Navigate somewhere to trigger the start of the worker to handle the
  // webNavigation.onBeforeRequest event.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  // Wait for the new worker instance to be added to `WorkerIdSet` (registered
  // in the process manager).
  {
    SCOPED_TRACE(
        "Waiting for worker to restart in response to extensions event.");
    worker_id_added_observer.WaitForInit();
  }

  std::vector<WorkerId> service_workers_after_restarted_worker =
      process_manager->GetServiceWorkersForExtension(extension->id());
  // TODO(crbug.com/40936639): Once this bug is fixed, enable this expect.
  // EXPECT_EQ(service_workers_after_restarted_worker.size(), 1u);
  EXPECT_EQ(service_workers_after_restarted_worker.size(), 2u);
  // Confirm `WorkerId` being tracked seems to be a new started instance than
  // the first one (WorkerIds are sorted by their attributes so the last is
  // considered the newest WorkerId since it has a higher thread, or process id,
  // etc.).
  const WorkerId& newly_started_service_worker_id =
      service_workers_after_restarted_worker.back();
  EXPECT_NE(newly_started_service_worker_id, previous_service_worker_id);
}

}  // namespace

}  // namespace extensions
