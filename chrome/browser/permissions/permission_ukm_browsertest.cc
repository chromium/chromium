// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace permissions {

class PermissionUkmBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(
        PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

  void TearDownOnMainThread() override {
    prompt_factory_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
};

IN_PROC_BROWSER_TEST_F(PermissionUkmBrowserTest, IgnoredPromptRecordedInUkm) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url1 = embedded_test_server()->GetURL("a.com", "/a.html");
  GURL url2 = embedded_test_server()->GetURL("b.com", "/b.html");

  // Navigate to the first URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  content::WebContents* web_contents =

      browser()->tab_strip_model()->GetActiveWebContents();
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents);

  // Trigger a permission prompt.
  std::unique_ptr<MockPermissionRequest> request =
      std::make_unique<MockPermissionRequest>(
          RequestType::kNotifications, PermissionRequestGestureType::GESTURE);
  manager->AddRequest(web_contents->GetPrimaryMainFrame(), std::move(request));

  prompt_factory_->WaitForPermissionBubble();
  ASSERT_TRUE(prompt_factory_->is_visible());

  // Navigate away.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  // The prompt should be closed (ignored).
  ASSERT_FALSE(prompt_factory_->is_visible());

  // Verify UKM.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Permission::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries.front().get();

  ukm_recorder.ExpectEntrySourceHasUrl(entry, url1);
  ukm_recorder.ExpectEntryMetric(
      entry, "Action", static_cast<int64_t>(PermissionAction::IGNORED));
}

}  // namespace permissions
