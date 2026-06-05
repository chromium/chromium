// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

class ContextualTasksUiServicePdfBrowserTest : public PDFExtensionTestBase {
 public:
  ContextualTasksUiServicePdfBrowserTest() = default;

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back({contextual_tasks::kContextualTasks, {}});
    enabled.push_back({contextual_tasks::kContextualTasksPdfCitations, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServicePdfBrowserTest,
                       PdfCitation_SeeksInsteadOfNavigating) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTask task = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  std::string pdf_url_with_fragment = std::string(url.spec()) + "#page=3";
  const GURL citation_url(pdf_url_with_fragment);

  extensions::TestEventRouterObserver observer(
      extensions::EventRouter::Get(browser()->profile()));

  ui_service->OnThreadLinkClicked(citation_url, task.GetTaskId(), nullptr,
                                  browser()->GetWeakPtr(), url::Origin());

  observer.WaitForEventWithName(
      extensions::api::pdf_viewer_private::OnShouldUpdateViewport::kEventName);
  EXPECT_EQ(1u, observer.dispatched_events().size());

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServicePdfBrowserTest,
                       PdfCitation_NotHandledOpensNewTab) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTask task = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  GURL bad_url("https://example.com/other.pdf#page=2");

  ui_service->OnThreadLinkClicked(bad_url, task.GetTaskId(), nullptr,
                                  browser()->GetWeakPtr(), url::Origin());

  // Wait for the new tab to be created.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return browser()->tab_strip_model()->count() == 2; }));

  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
}

}  // namespace contextual_tasks
