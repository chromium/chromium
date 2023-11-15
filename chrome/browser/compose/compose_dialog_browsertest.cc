// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_session.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace compose {

//  IDC_CONTEXT_COMPOSE

class ComposeSessionBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list()->InitWithFeatures(
        {compose::features::kEnableCompose,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
    InProcessBrowserTest::SetUp();
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_MAC)
// Mac failures: b/311208586
#define MAYBE_LifetimeOfBubbleWrapper DISABLED_LifetimeOfBubbleWrapper
#else
#define MAYBE_LifetimeOfBubbleWrapper LifetimeOfBubbleWrapper
#endif
IN_PROC_BROWSER_TEST_F(ComposeSessionBrowserTest,
                       MAYBE_LifetimeOfBubbleWrapper) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test2.html")));
  ASSERT_NE(nullptr, ChromeComposeClient::FromWebContents(web_contents));
  auto* client = ChromeComposeClient::FromWebContents(web_contents);
  client->GetComposeEnabling().SetEnabledForTesting();

  // get point of element
  gfx::PointF textarea_center =
      content::GetCenterCoordinatesOfElementWithId(web_contents, "elem1");
  autofill::FormFieldData field_data;
  field_data.bounds = gfx::RectF((textarea_center), gfx::SizeF(1, 1));

  client->ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      field_data, std::nullopt, base::NullCallback());

  // close window right away
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
}

}  // namespace compose
