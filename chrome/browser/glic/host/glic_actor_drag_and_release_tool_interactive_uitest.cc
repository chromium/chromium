// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, DragAndReleaseTool_Range) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/drag.html");

  gfx::Rect range_rect;
  auto drag_provider = base::BindLambdaForTesting([this, &range_rect]() {
    // Padding to roughly hit the center of the range drag thumb.
    const int thumb_padding = range_rect.height() / 2;

    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);

    gfx::Point end = range_rect.CenterPoint();

    Actions action = actor::MakeDragAndRelease(tab_handle_, start, end);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, "range", range_rect),
      CheckJsResult(kNewActorTabId,
                    "() => document.querySelector('#range').value", "0"),
      ExecuteAction(std::move(drag_provider)),
      CheckJsResult(kNewActorTabId,
                    "() => document.querySelector('#range').value", "50"));
}

// Test coordinate conversions under normal and high-DPI scaling factors.
class GlicActorDragDSFTest : public GlicActorUiTest,
                             public testing::WithParamInterface<int> {
 public:
  GlicActorDragDSFTest() {
    display::Display::SetForceDeviceScaleFactor(DeviceScaleFactor());
    // TODO (crbug.com/454665367): This test only fails on Windows when the
    // Multi-Instance flag is enabled. Needs further investigation as to why.
    scoped_feature_list_.InitAndDisableFeature(features::kGlicMultiInstance);
  }
  ~GlicActorDragDSFTest() override = default;

  int DeviceScaleFactor() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure the drag tool sends the expected mouse down, move and up events.
IN_PROC_BROWSER_TEST_P(GlicActorDragDSFTest, Events) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/drag.html");

  // The values are provided in DIPs. Since there is no browser zoom, this is
  // equivalent to CSS pixels so should be the same values provided to web APIs,
  // regardless of device scale.
  const gfx::Vector2d delta(100, 150);
  const gfx::Point start(10, 20);
  const gfx::Point end = start + delta;

  auto drag_provider = base::BindLambdaForTesting([this, start, end]() {
    Actions action = actor::MakeDragAndRelease(tab_handle_, start, end);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      CheckJsResult(kNewActorTabId, "() => window.devicePixelRatio",
                    DeviceScaleFactor()),
      ExecuteJs(kNewActorTabId, R"JS(
          () => {
              document.getElementById('dragLogger')
                  .scrollIntoView({block:'start', inline:'start'});
          })JS"),
      CheckJsResult(kNewActorTabId, "() => event_log.join(',')", ""),
      ExecuteAction(std::move(drag_provider)),
      CheckJsResult(kNewActorTabId, "() => event_log.join(',')",
                    testing::AllOf(testing::StartsWith(absl::StrFormat(
                                       "mousemove[%s],mousedown[%s],",
                                       start.ToString(), start.ToString())),
                                   testing::EndsWith(absl::StrFormat(
                                       "mousemove[%s],mouseup[%s]",
                                       end.ToString(), end.ToString())))));
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicActorDragDSFTest,
                         testing::Values(1, 2),
                         [](const ::testing::TestParamInfo<int>& info) {
                           return absl::StrFormat("DSF_%d", info.param);
                         });

// Ensure coordinates outside of the viewport are rejected.
IN_PROC_BROWSER_TEST_F(GlicActorUiTest, DragAndReleaseTool_Offscreen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/drag.html");

  gfx::Rect range_rect;
  auto drag_provider = base::BindLambdaForTesting([this, &range_rect]() {
    // Padding to roughly hit the center of the range drag thumb.
    const int thumb_padding = range_rect.height() / 2;

    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);

    gfx::Point end = range_rect.CenterPoint();

    Actions action = actor::MakeDragAndRelease(tab_handle_, start, end);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      CheckJsResult(kNewActorTabId, "() => event_log.join(',')", ""),
      GetClientRect(kNewActorTabId, "offscreenRange", range_rect),
      ExecuteAction(drag_provider,
                    actor::mojom::ActionResultCode::kCoordinatesOutOfBounds),

      // Scroll the range into the viewport.
      ExecuteJs(kNewActorTabId,
                "() => { "
                "document.getElementById('offscreenRange').scrollIntoView(); "
                "}"),

      // Try to drag the range again - it should succeed now.
      GetClientRect(kNewActorTabId, "offscreenRange", range_rect),
      ExecuteAction(drag_provider),
      CheckJsResult(kNewActorTabId,
                    "() => document.querySelector('#offscreenRange').value",
                    "50"));
}

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, DragAndReleaseTool_DOMNodeId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/drag_dom_node_id.html");

  auto drag_provider = base::BindLambdaForTesting([this]() {
    int32_t from_node_id = SearchAnnotatedPageContent("fromTarget");
    int32_t to_node_id = SearchAnnotatedPageContent("toTarget");
    content::RenderFrameHost* frame =
        tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
    Actions action =
        actor::MakeDragAndRelease(*frame, from_node_id, to_node_id);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      CheckJsResult(kNewActorTabId, "() => event_log.join(',')", ""),
      ExecuteAction(std::move(drag_provider)),
      CheckJsResult(
          kNewActorTabId, "() => event_log.join(',')",
          testing::AllOf(
              testing::StartsWith("mousemove#fromTarget,mousedown#fromTarget,"),
              testing::EndsWith("mousemove#toTarget,mouseup#toTarget"))));
}

}  // namespace

}  // namespace glic::test
