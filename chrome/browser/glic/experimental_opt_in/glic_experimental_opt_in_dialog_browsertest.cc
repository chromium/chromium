// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"

namespace glic {

class GlicExperimentalOptInTest : public GlicBrowserTest {
 public:
  GlicExperimentalOptInTest() = default;
  ~GlicExperimentalOptInTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, OpensDialog) {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  views::Widget* widget = service->opt_in_controller().ShowDialog(web_contents);
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  service->opt_in_controller().CloseDialog();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, TabModality) {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());

  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  views::Widget* widget = service->opt_in_controller().ShowDialog(tab1);
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Open a new tab.
  chrome::AddSelectedTabWithURL(browser(), GURL("about:blank"),
                                ui::PAGE_TRANSITION_LINK);
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(tab1, tab2);

  // The dialog should be hidden.
  EXPECT_FALSE(widget->IsVisible());

  // Switch back to tab1.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(tab1, browser()->tab_strip_model()->GetActiveWebContents());

  // The dialog should be visible again.
  EXPECT_TRUE(widget->IsVisible());

  // Cleanup.
  service->opt_in_controller().CloseDialog();
}

}  // namespace glic
