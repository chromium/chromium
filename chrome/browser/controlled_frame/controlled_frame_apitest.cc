// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/service_worker_apitest.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace controlled_frame {

class ControlledFrameServiceWorkerTest
    : public extensions::ServiceWorkerBasedBackgroundTest {
 public:
  ControlledFrameServiceWorkerTest(const ControlledFrameServiceWorkerTest&) =
      delete;
  ControlledFrameServiceWorkerTest& operator=(
      const ControlledFrameServiceWorkerTest&) = delete;

 protected:
  ControlledFrameServiceWorkerTest() {
    feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kIsolatedWebApps,
                              features::kIsolatedWebAppDevMode,
                              features::kIwaControlledFrame},
        /*disabled_features=*/{});
  }

  ~ControlledFrameServiceWorkerTest() override = default;

  base::test::ScopedFeatureList feature_list;
};

// This test ensures that loading an extension Service Worker does not cause a
// crash, and that Controlled Frame is not allowed in the Service Worker
// context. For more details, see https://crbug.com/1462384.
// This test is the same as ServiceWorkerBasedBackgroundTest.Basic.
IN_PROC_BROWSER_TEST_F(ControlledFrameServiceWorkerTest, PRE_Basic) {
  base::HistogramTester histogram_tester;
  ExtensionTestMessageListener newtab_listener("CREATED");
  newtab_listener.set_failure_message("CREATE_FAILED");
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING");
  worker_listener.set_failure_message("NON_WORKER_SCOPE");
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "service_worker/worker_based_background/basic"));
  ASSERT_TRUE(extension);
  const extensions::ExtensionId extension_id = extension->id();
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  content::WebContents* new_web_contents =
      extensions::browsertest_util::AddTab(browser(), url);
  EXPECT_TRUE(new_web_contents);
  EXPECT_TRUE(newtab_listener.WaitUntilSatisfied());

  // Service Worker extension does not have ExtensionHost.
  EXPECT_FALSE(process_manager()->GetBackgroundHostForExtension(extension_id));

  // Call to runtime.onInstalled and tabs.onCreated are expected.
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*expected_count=*/2);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
      /*expected_count=*/2);
  histogram_tester.ExpectTotalCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker",
      /*expected_count=*/2);

  // Verify that the recorded values are sane -- that is, that they are less
  // than the maximum bucket.
  // This is the best we can do, since the other buckets are determined
  // by the histogram, rather than by us.
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckTime.ExtensionServiceWorker2",
      /*sample=*/base::Minutes(5).InMicroseconds(), /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DispatchToAckLongTime.ExtensionServiceWorker2",
      /*sample=*/base::Days(1).InMilliseconds(), /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "Extensions.Events.DidDispatchToAckSucceed.ExtensionServiceWorker",
      /*sample=*/false, /*expected_count=*/0);
}

// After browser restarts, this test step ensures that opening a tab fires
// tabs.onCreated event listener to the extension without explicitly loading the
// extension. This is because the extension registered a listener before browser
// restarted in PRE_Basic.
IN_PROC_BROWSER_TEST_F(ControlledFrameServiceWorkerTest, Basic) {
  ExtensionTestMessageListener newtab_listener("CREATED");
  newtab_listener.set_failure_message("CREATE_FAILED");
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  content::WebContents* new_web_contents =
      extensions::browsertest_util::AddTab(browser(), url);
  EXPECT_TRUE(new_web_contents);
  EXPECT_TRUE(newtab_listener.WaitUntilSatisfied());
}
}  // namespace controlled_frame
