// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using ClickAction = apc::ClickAction;
using apc::AnnotatedPageContent;
using apc::BoundingRect;
using apc::ContentNode;

class GlicActorPopupUiTest : public GlicActorUiTest,
                             public testing::WithParamInterface<bool> {
 public:
  static constexpr double kDeviceScaleFactor = 2.0;
  GlicActorPopupUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlicActorInternalPopups,
            blink::features::kAIPageContentIncludePopupWindows,
        },
        /*disabled_features=*/{});
    if (GetParam()) {
      display::Display::SetForceDeviceScaleFactor(kDeviceScaleFactor);
    }
  }
  ~GlicActorPopupUiTest() override = default;

  float GetDeviceScaleFactor() const { return GetParam() ? 2.0 : 1.0; }

  // Searches the popup in the APC for the parent of the text node containing
  // the provided text.
  const ContentNode* SearchAnnotatedPageContentForTextParentInPopup(
      std::string_view text) {
    CHECK(annotated_page_content_)
        << "An observation must be made with GetPageContextForActorTab "
           "before searching annotated page content.";
    CHECK(annotated_page_content_->has_popup_window());

    // Traverse the APC in depth-first preorder, returning the parent of the
    // first node that matches the given text.
    std::stack<const ContentNode*> nodes;
    nodes.push(&annotated_page_content_->popup_window().root_node());

    while (!nodes.empty()) {
      const ContentNode* current = nodes.top();
      nodes.pop();

      for (const auto& child : current->children_nodes()) {
        if (child.content_attributes().text_data().text_content() == text) {
          return current;
        }

        nodes.push(&child);
      }
    }

    // Tests must pass text that matches one of the content nodes.
    NOTREACHED() << "Text [" << text << "] not found in page.";
  }

  // Select popups are implemented with an inner listbox-type <select> and this
  // helper finds the APC node for that inner <select> within the popup.
  const ContentNode* SearchAnnotatedPageContentForSelectInPopup() {
    CHECK(annotated_page_content_)
        << "An observation must be made with GetPageContextForActorTab "
           "before searching annotated page content.";
    CHECK(annotated_page_content_->has_popup_window());

    // Traverse the APC in depth-first preorder, returning the parent of the
    // first node that matches the given text.
    std::stack<const ContentNode*> nodes;
    nodes.push(&annotated_page_content_->popup_window().root_node());

    while (!nodes.empty()) {
      const ContentNode* current = nodes.top();
      nodes.pop();

      if (current->content_attributes().attribute_type() ==
              optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL &&
          current->content_attributes()
                  .form_control_data()
                  .form_control_type() ==
              optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_ONE) {
        return current;
      }

      for (const auto& child : current->children_nodes()) {
        nodes.push(&child);
      }
    }

    // Tests must pass text that matches one of the content nodes.
    NOTREACHED() << "Select [] not found in page.";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicActorPopupUiTest, ActOnPopupWidgetWithId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  constexpr std::string_view kPlainSelect = "plainSelect";
  constexpr std::string_view kSelectLabel = "plain-select";
  constexpr std::string_view kSelectPopupText = "beta";

  const std::string kGetValueScript = content::JsReplace(
      "() => document.getElementById($1).value", kPlainSelect);

  auto click_provider = base::BindLambdaForTesting([&kSelectPopupText, this]() {
    const ContentNode* element =
        SearchAnnotatedPageContentForTextParentInPopup(kSelectPopupText);
    content::RenderFrameHost* frame =
        tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
    apc::Actions action = actor::MakeClick(
        *frame, element->content_attributes().common_ancestor_dom_node_id(),
        apc::ClickAction::LEFT, apc::ClickAction::SINGLE);

    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  // clang-format off
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),

      // The select box starts with the "alpha" option selected
      CheckJsResult(kNewActorTabId, kGetValueScript, "alpha"),

      // Open a popup <select> control by clicking on it
      ClickAction(kSelectLabel, ClickAction::LEFT, ClickAction::SINGLE),
      GetPageContextForActorTab(),

      // Click on a new option.
      ExecuteAction(std::move(click_provider)),
      CheckJsResult(kNewActorTabId, kGetValueScript, "beta"));
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(GlicActorPopupUiTest, ActOnPopupWidgetWithCoords) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  constexpr std::string_view kPlainSelect = "plainSelect";
  constexpr std::string_view kSelectLabel = "plain-select";
  constexpr std::string_view kSelectPopupText = "beta";

  const std::string kGetValueScript = content::JsReplace(
      "() => document.getElementById($1).value", kPlainSelect);

  auto click_provider = base::BindLambdaForTesting([&kSelectPopupText, this]() {
    const ContentNode* element =
        SearchAnnotatedPageContentForTextParentInPopup(kSelectPopupText);
    const BoundingRect& bbox =
        element->content_attributes().geometry().visible_bounding_box();

    gfx::Point coordinate(bbox.x() + bbox.width() / 2,
                          bbox.y() + bbox.height() / 2);
    // Convert to DIPs
    coordinate =
        gfx::ScaleToRoundedPoint(coordinate, 1.0 / GetDeviceScaleFactor());
    apc::Actions action =
        actor::MakeClick(tab_handle_, coordinate, apc::ClickAction::LEFT,
                         apc::ClickAction::SINGLE);

    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  // clang-format off
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),

      // The select box starts with the "alpha" option selected
      CheckJsResult(kNewActorTabId, kGetValueScript, "alpha"),

      // Open a popup <select> control by clicking on it
      ClickAction(kSelectLabel, ClickAction::LEFT, ClickAction::SINGLE),
      GetPageContextForActorTab(),

      // Click on a new option.
      ExecuteAction(std::move(click_provider)),
      CheckJsResult(kNewActorTabId, kGetValueScript, "beta"));
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(GlicActorPopupUiTest, ActOnPopupWidgetWithSelectTool) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  constexpr std::string_view kPlainSelect = "plainSelect";
  constexpr std::string_view kSelectLabel = "plain-select";
  // For the popup's select box the element's values are the 0-based index into
  // the original select box. "beta" is the second element.
  constexpr std::string_view kSelectPopupText = "1";

  const std::string kGetValueScript = content::JsReplace(
      "() => document.getElementById($1).value", kPlainSelect);

  auto select_provider =
      base::BindLambdaForTesting([&kSelectPopupText, this]() {
        const ContentNode* element =
            SearchAnnotatedPageContentForSelectInPopup();
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        apc::Actions action = actor::MakeSelect(
            *frame, element->content_attributes().common_ancestor_dom_node_id(),
            kSelectPopupText);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  // clang-format off
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),

      // The select box starts with the "alpha" option selected
      CheckJsResult(kNewActorTabId, kGetValueScript, "alpha"),

      // Open a popup <select> control by clicking on it
      ClickAction(kSelectLabel, ClickAction::LEFT, ClickAction::SINGLE),
      GetPageContextForActorTab(),

      // Click on a new option.
      ExecuteAction(std::move(select_provider)),
      WaitForJsResult(kNewActorTabId, kGetValueScript, "beta"));
  // clang-format on
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicActorPopupUiTest,
                         testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "2x" : "1x";
                         });

}  // namespace

}  // namespace glic::test
