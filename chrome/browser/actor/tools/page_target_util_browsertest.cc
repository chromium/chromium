// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_target_util.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace actor {

class PageTargetUtilSecurityTest : public ActorToolsTest {
 public:
  PageTargetUtilSecurityTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAIPageContentIncludePopupWindows);
  }

 protected:
  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  tabs::TabInterface* active_tab() {
    return browser()->tab_strip_model()->GetActiveTab();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageTargetUtilSecurityTest,
                       GetFieldIdFromPageTarget_SpoofedPopupRedirectsToOOPIF) {
  // 1. Load a page with an OOPIF.
  GURL main_url = embedded_https_test_server().GetURL("a.com", "/iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  GURL iframe_url =
      embedded_https_test_server().GetURL("b.com", "/title1.html");
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(), "test", iframe_url));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame->IsCrossProcessSubframe());

  // Wait for the hit test data to be fully propagated to the browser-side
  // router to avoid flakiness in FindWidgetAtPoint.
  content::WaitForHitTestData(child_frame);

  // 2. Create a spoofed APC with a popup_window belonging to the attacker
  // (child_frame).
  optimization_guide::proto::AnnotatedPageContent apc;
  auto* popup = apc.mutable_popup_window();

  // Attacker frame info
  auto* user_data = optimization_guide::DocumentIdentifierUserData::
      GetOrCreateForCurrentDocument(child_frame);
  std::string attacker_doc_id = user_data->serialized_token();

  popup->mutable_opener_document_id()->set_serialized_token(attacker_doc_id);
  auto* root_node = popup->mutable_root_node();
  root_node->mutable_content_attributes()->set_common_ancestor_dom_node_id(
      4242);
  root_node->mutable_content_attributes()
      ->mutable_interaction_info()
      ->set_document_scoped_z_order(1);

  // Set popup bounds to overlap a point in the main frame.
  // In APC, geometry is in visual-viewport-relative device pixels (BlinkSpace).
  // For simplicity in test, assume DSF=1.0 and no viewport offset.
  auto* geometry = root_node->mutable_content_attributes()->mutable_geometry();
  auto* box = geometry->mutable_visible_bounding_box();
  box->set_x(350);
  box->set_y(0);
  box->set_width(100);
  box->set_height(100);

  // 3. Test a point (400, 50) which is in the main frame but covered by the
  // spoofed popup.
  gfx::Point target_point(400, 50);

  // Compositor-trusted hit test should return main frame's widget.
  content::RenderWidgetHost* actual_rwh =
      web_contents()->FindWidgetAtPoint(gfx::PointF(target_point));
  ASSERT_EQ(actual_rwh, main_frame->GetRenderWidgetHost());

  // GetFieldIdFromPageTarget should return empty because of the fix.
  // If vulnerable, it would return the attacker's field ID.
  autofill::FieldGlobalId resolved_id =
      GetFieldIdFromPageTarget(&apc, active_tab(), target_point);

  EXPECT_FALSE(resolved_id);
}

}  // namespace actor
