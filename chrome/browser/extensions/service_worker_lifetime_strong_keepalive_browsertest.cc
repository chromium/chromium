// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr char kTestOpenerExtensionId[] = "adpghjkjicpfhcjicmiifjpbalaildpo";
constexpr char kTestOpenerExtensionUrl[] =
    "chrome-extension://adpghjkjicpfhcjicmiifjpbalaildpo/";
constexpr char kTestOpenerExtensionRelativePath[] =
    "service_worker/policy/opener_extension";

constexpr char kTestReceiverExtensionId[] = "eagjmgdicfmccfhiiihnaehbfheheidk";
constexpr char kTestReceiverExtensionUrl[] =
    "chrome-extension://eagjmgdicfmccfhiiihnaehbfheheidk/";
constexpr char kTestReceiverExtensionRelativePath[] =
    "service_worker/policy/receiver_extension";

constexpr char kPersistentPortConnectedMessage[] = "Persistent port connected";
constexpr char kPersistentPortDisconnectedMessage[] =
    "Persistent port disconnected";

content::ServiceWorkerContext* GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return browser_context->GetDefaultStoragePartition()
      ->GetServiceWorkerContext();
}

}  // namespace

// Observer for an extension service worker to start and stop.
class TestServiceWorkerContextObserver
    : public content::ServiceWorkerContextObserver {
 public:
  TestServiceWorkerContextObserver(content::ServiceWorkerContext* context,
                                   const ExtensionId& extension_id)
      : context_(context),
        extension_url_(Extension::GetBaseURLFromExtensionId(extension_id)) {
    scoped_observation_.Observe(context);
  }

  TestServiceWorkerContextObserver(const TestServiceWorkerContextObserver&) =
      delete;
  TestServiceWorkerContextObserver& operator=(
      const TestServiceWorkerContextObserver&) = delete;

  ~TestServiceWorkerContextObserver() override = default;

  void WaitForWorkerStart() {
    started_run_loop_.Run();
    EXPECT_TRUE(running_version_id_.has_value());
  }

  void WaitForWorkerStop() {
    // OnVersionStoppedRunning() might have already cleared running_version_id_.
    if (running_version_id_.has_value()) {
      stopped_run_loop_.Run();
    }
  }

  int64_t GetServiceWorkerVersionId() { return running_version_id_.value(); }

 private:
  // ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (running_info.scope != extension_url_) {
      return;
    }

    running_version_id_ = version_id;
    started_run_loop_.Quit();
  }

  void OnVersionStoppedRunning(int64_t version_id) override {
    if (running_version_id_ == version_id) {
      stopped_run_loop_.Quit();
    }
    running_version_id_ = absl::nullopt;
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    DCHECK(scoped_observation_.IsObserving());
    scoped_observation_.Reset();
  }

  base::RunLoop stopped_run_loop_;
  base::RunLoop started_run_loop_;
  absl::optional<int64_t> running_version_id_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
  raw_ptr<content::ServiceWorkerContext> context_ = nullptr;
  GURL extension_url_;
};

class ServiceWorkerLifetimeStrongKeepaliveBrowsertest
    : public ExtensionApiTest {
 public:
  ServiceWorkerLifetimeStrongKeepaliveBrowsertest() = default;

  ServiceWorkerLifetimeStrongKeepaliveBrowsertest(
      const ServiceWorkerLifetimeStrongKeepaliveBrowsertest&) = delete;
  ServiceWorkerLifetimeStrongKeepaliveBrowsertest& operator=(
      const ServiceWorkerLifetimeStrongKeepaliveBrowsertest&) = delete;

  ~ServiceWorkerLifetimeStrongKeepaliveBrowsertest() override = default;

  void TriggerTimeoutAndCheckActive(content::ServiceWorkerContext* context,
                                    int64_t version_id) {
    EXPECT_TRUE(
        content::TriggerTimeoutAndCheckRunningState(context, version_id));
  }

  void TriggerTimeoutAndCheckStopped(content::ServiceWorkerContext* context,
                                     int64_t version_id) {
    EXPECT_FALSE(
        content::TriggerTimeoutAndCheckRunningState(context, version_id));
  }

  base::SimpleTestTickClock tick_clock_opener;
  base::SimpleTestTickClock tick_clock_receiver;
};

// Loads two extensions that open a persistent port connection between each
// other and tests that their service worker will stop after kRequestTimeout (5
// minutes).
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeStrongKeepaliveBrowsertest,
                       ServiceWorkersTimeOutWithoutPolicy) {
  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver);
  TriggerTimeoutAndCheckStopped(context, service_worker_receiver_id);
  sw_observer_receiver_extension.WaitForWorkerStop();

  // Advance clock and check that the opener service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener);
  TriggerTimeoutAndCheckStopped(context, service_worker_opener_id);
  sw_observer_opener_extension.WaitForWorkerStop();
}

// Tests that the service workers will not stop if both extensions are
// allowlisted via policy and the port is not closed.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeStrongKeepaliveBrowsertest,
                       ServiceWorkersDoNotTimeOutWithPolicy) {
  base::Value::List urls;
  // Both extensions receive extended lifetime.
  urls.Append(kTestOpenerExtensionUrl);
  urls.Append(kTestReceiverExtensionUrl);
  browser()->profile()->GetPrefs()->SetList(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
      std::move(urls));

  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  const Extension* receiver_extension = LoadExtension(
      test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  const Extension* opener_extension = LoadExtension(
      test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  // Advance clock and check that the receiver service worker did not stop.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver);
  TriggerTimeoutAndCheckActive(context, service_worker_receiver_id);

  // Advance clock and check that the opener service worker did not stop.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener);
  TriggerTimeoutAndCheckActive(context, service_worker_opener_id);

  // Clean up: stop running service workers before test end.
  base::test::TestFuture<void> future_1;
  content::StopServiceWorkerForScope(context, receiver_extension->url(),
                                     future_1.GetCallback());
  EXPECT_TRUE(future_1.Wait());

  base::test::TestFuture<void> future_2;
  content::StopServiceWorkerForScope(context, opener_extension->url(),
                                     future_2.GetCallback());
  EXPECT_TRUE(future_2.Wait());
}

// Tests that the extended lifetime only lasts as long as there is a persistent
// port connection. If the port is closed (by one of the service workers
// stopping), the other service worker will also stop, even if it received an
// extended lifetime.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeStrongKeepaliveBrowsertest,
                       ServiceWorkersTimeOutWhenOnlyOneHasExtendedLifetime) {
  base::Value::List urls;
  // Opener extension will receive extended lifetime because it connects to a
  // policy allowlisted extension.
  urls.Append(kTestReceiverExtensionUrl);
  browser()->profile()->GetPrefs()->SetList(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
      std::move(urls));

  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  ExtensionTestMessageListener disconnect_listener(
      kPersistentPortDisconnectedMessage);
  disconnect_listener.set_extension_id(kTestOpenerExtensionId);

  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver);
  TriggerTimeoutAndCheckStopped(context, service_worker_receiver_id);

  // Wait for the receiver SW to be closed in order for the port to be
  // disconnected and the opener SW losing extended lifetime.
  sw_observer_receiver_extension.WaitForWorkerStop();

  // Wait for port to close in the opener extension.
  ASSERT_TRUE(disconnect_listener.WaitUntilSatisfied());

  // Advance clock and check that the opener service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener);
  TriggerTimeoutAndCheckStopped(context, service_worker_opener_id);
  sw_observer_opener_extension.WaitForWorkerStop();
}

// Tests that the service workers will stop if both extensions are allowlisted
// via policy and the port is disconnected.
IN_PROC_BROWSER_TEST_F(ServiceWorkerLifetimeStrongKeepaliveBrowsertest,
                       ServiceWorkersTimeOutWhenPortIsDisconnected) {
  base::Value::List urls;
  // Both extensions receive extended lifetime.
  urls.Append(kTestReceiverExtensionUrl);
  urls.Append(kTestOpenerExtensionUrl);
  browser()->profile()->GetPrefs()->SetList(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
      std::move(urls));

  content::ServiceWorkerContext* context =
      GetServiceWorkerContext(browser()->profile());

  TestServiceWorkerContextObserver sw_observer_receiver_extension(
      context, kTestReceiverExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestReceiverExtensionRelativePath));
  sw_observer_receiver_extension.WaitForWorkerStart();

  ExtensionTestMessageListener connect_listener(
      kPersistentPortConnectedMessage);
  connect_listener.set_extension_id(kTestReceiverExtensionId);

  TestServiceWorkerContextObserver sw_observer_opener_extension(
      context, kTestOpenerExtensionId);
  LoadExtension(test_data_dir_.AppendASCII(kTestOpenerExtensionRelativePath));
  sw_observer_opener_extension.WaitForWorkerStart();

  ASSERT_TRUE(connect_listener.WaitUntilSatisfied());

  int64_t service_worker_receiver_id =
      sw_observer_receiver_extension.GetServiceWorkerVersionId();
  int64_t service_worker_opener_id =
      sw_observer_opener_extension.GetServiceWorkerVersionId();

  ExtensionTestMessageListener disconnect_listener(
      kPersistentPortDisconnectedMessage);
  disconnect_listener.set_extension_id(kTestOpenerExtensionId);

  // Disconnect the port from the receiver extension.
  constexpr char kDisconnectScript[] = R"(port.disconnect();)";
  BackgroundScriptExecutor script_executor(browser()->profile());
  script_executor.ExecuteScriptAsync(
      kTestReceiverExtensionId, kDisconnectScript,
      BackgroundScriptExecutor::ResultCapture::kNone,
      browsertest_util::ScriptUserActivation::kDontActivate);

  // Wait for port to close in the opener extension.
  ASSERT_TRUE(disconnect_listener.WaitUntilSatisfied());

  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_receiver_id,
                                           &tick_clock_receiver);
  TriggerTimeoutAndCheckStopped(context, service_worker_receiver_id);

  // Wait for the receiver SW to be closed.
  sw_observer_receiver_extension.WaitForWorkerStop();

  // Advance clock and check that the opener service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_opener_id,
                                           &tick_clock_opener);
  TriggerTimeoutAndCheckStopped(context, service_worker_opener_id);
  sw_observer_opener_extension.WaitForWorkerStop();
}

}  // namespace extensions
