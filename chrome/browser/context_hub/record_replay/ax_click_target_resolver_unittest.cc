// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/ax_click_target_resolver.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

class AxClickTargetResolverTest : public ChromeRenderViewHostTestHarness {
 public:
  AxClickTargetResolverTest() = default;
  AxClickTargetResolverTest(const AxClickTargetResolverTest&) = delete;
  AxClickTargetResolverTest& operator=(const AxClickTargetResolverTest&) =
      delete;
  ~AxClickTargetResolverTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));
  }

  void TearDown() override {
    ax_tree_managers_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetAXTreeForTreeId(const ui::AXTreeID& tree_id,
                          ui::AXTreeUpdate update) {
    update.has_tree_data = true;
    update.tree_data.tree_id = tree_id;
    ax_tree_managers_.erase(tree_id);
    ax_tree_managers_.emplace(tree_id,
                              std::make_unique<ui::AXTreeManager>(
                                  std::make_unique<ui::AXTree>(update)));
  }

  void SetMainFrameAXTree(ui::AXTreeUpdate update) {
    SetAXTreeForTreeId(main_rfh()->GetAXTreeID(), std::move(update));
  }

  std::optional<std::string> ResolveAt(float x, float y) {
    return ResolveClickedElementLabel(*main_rfh(), main_rfh(),
                                      gfx::PointF(x, y));
  }

  std::map<ui::AXTreeID, std::unique_ptr<ui::AXTreeManager>> ax_tree_managers_;
};

TEST_F(AxClickTargetResolverTest,
       ReturnsNulloptForMissingTreeNullTargetOrCrossOriginSubframe) {
  // Missing `AXTreeManager` for `main_rfh()`.
  EXPECT_EQ(ResolveAt(10.0f, 10.0f), std::nullopt);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2};

  ui::AXNodeData button;
  button.id = 2;
  button.role = ax::mojom::Role::kButton;
  button.SetNameChecked("Submit");
  button.relative_bounds.bounds = gfx::RectF(0, 0, 100, 40);

  ui::AXTreeUpdate main_update;
  main_update.root_id = 1;
  main_update.nodes = {root, button};
  SetMainFrameAXTree(main_update);

  EXPECT_EQ(
      ResolveClickedElementLabel(*main_rfh(), nullptr, gfx::PointF(10, 10)),
      std::nullopt);
  EXPECT_EQ(ResolveAt(10.0f, 10.0f), "Submit");

  content::RenderFrameHost* cross_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("cross");
  cross_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.example/sub"), cross_rfh);
  SetAXTreeForTreeId(cross_rfh->GetAXTreeID(), main_update);
  EXPECT_EQ(
      ResolveClickedElementLabel(*main_rfh(), cross_rfh, gfx::PointF(10, 10)),
      std::nullopt);
}

TEST_F(AxClickTargetResolverTest,
       ResolvesInteractiveControlsAndFiltersNonInteractiveAndPasswordNodes) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2};

  // Full-viewport container with a delegated click listener (`#root`).
  ui::AXNodeData app_root;
  app_root.id = 2;
  app_root.role = ax::mojom::Role::kGenericContainer;
  app_root.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  app_root.SetNameChecked("App Root");
  app_root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  app_root.child_ids = {3, 4, 5, 6};

  ui::AXNodeData plain_text;
  plain_text.id = 3;
  plain_text.role = ax::mojom::Role::kStaticText;
  plain_text.SetNameChecked("Non-interactive paragraph");
  plain_text.relative_bounds.bounds = gfx::RectF(10, 10, 200, 20);

  ui::AXNodeData password_field;
  password_field.id = 4;
  password_field.role = ax::mojom::Role::kTextField;
  password_field.AddState(ax::mojom::State::kEditable);
  password_field.AddState(ax::mojom::State::kProtected);
  password_field.SetNameChecked("Password");
  password_field.relative_bounds.bounds = gfx::RectF(10, 60, 200, 30);

  ui::AXNodeData search_box;
  search_box.id = 5;
  search_box.role = ax::mojom::Role::kSearchBox;
  search_box.SetNameChecked("Search");
  search_box.relative_bounds.bounds = gfx::RectF(10, 110, 200, 30);

  ui::AXNodeData unnamed_button;
  unnamed_button.id = 6;
  unnamed_button.role = ax::mojom::Role::kButton;
  unnamed_button.relative_bounds.bounds = gfx::RectF(10, 160, 100, 30);

  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root,           app_root,   plain_text,
                  password_field, search_box, unnamed_button};
  SetMainFrameAXTree(std::move(update));

  EXPECT_EQ(ResolveAt(20.0f, 15.0f), std::nullopt);
  EXPECT_EQ(ResolveAt(20.0f, 70.0f), std::nullopt);
  EXPECT_EQ(ResolveAt(20.0f, 120.0f), "Search");
  EXPECT_EQ(ResolveAt(20.0f, 170.0f), "button");
}

TEST_F(AxClickTargetResolverTest, ResolvesClickInSameOriginChildIframe) {
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  child_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/child"), child_rfh);
  const ui::AXTreeID main_tree_id = main_rfh()->GetAXTreeID();
  const ui::AXTreeID child_tree_id = child_rfh->GetAXTreeID();

  ui::AXNodeData main_root;
  main_root.id = 1;
  main_root.role = ax::mojom::Role::kRootWebArea;
  main_root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  main_root.child_ids = {2};

  ui::AXNodeData iframe_node;
  iframe_node.id = 2;
  iframe_node.role = ax::mojom::Role::kIframe;
  iframe_node.relative_bounds.bounds = gfx::RectF(100, 100, 300, 200);
  iframe_node.AddChildTreeId(child_tree_id);

  ui::AXTreeUpdate main_update;
  main_update.root_id = 1;
  main_update.nodes = {main_root, iframe_node};
  SetMainFrameAXTree(std::move(main_update));

  ui::AXNodeData child_root;
  child_root.id = 1;
  child_root.role = ax::mojom::Role::kRootWebArea;
  child_root.relative_bounds.bounds = gfx::RectF(0, 0, 300, 200);
  child_root.child_ids = {2};

  ui::AXNodeData iframe_button;
  iframe_button.id = 2;
  iframe_button.role = ax::mojom::Role::kButton;
  iframe_button.SetNameChecked("Iframe Action");
  iframe_button.relative_bounds.bounds = gfx::RectF(10, 20, 120, 40);

  ui::AXTreeUpdate child_update;
  child_update.has_tree_data = true;
  child_update.tree_data.tree_id = child_tree_id;
  child_update.tree_data.parent_tree_id = main_tree_id;
  child_update.root_id = 1;
  child_update.nodes = {child_root, iframe_button};
  SetAXTreeForTreeId(child_tree_id, std::move(child_update));

  EXPECT_EQ(ResolveAt(105.0f, 105.0f), std::nullopt);
  EXPECT_EQ(ResolveAt(120.0f, 130.0f), "Iframe Action");
}

TEST_F(AxClickTargetResolverTest,
       ScopesHitTestingToModalDialogOverOccludedBackgroundControls) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2, 3, 4, 5, 7};

  ui::AXNodeData outside_button;
  outside_button.id = 2;
  outside_button.role = ax::mojom::Role::kButton;
  outside_button.SetNameChecked("Outside Action");
  outside_button.relative_bounds.bounds = gfx::RectF(10, 10, 80, 30);

  // Background button positioned underneath empty space of `dialog`.
  ui::AXNodeData occluded_backdrop_button;
  occluded_backdrop_button.id = 3;
  occluded_backdrop_button.role = ax::mojom::Role::kButton;
  occluded_backdrop_button.SetNameChecked("Occluded Under Dialog");
  occluded_backdrop_button.relative_bounds.bounds =
      gfx::RectF(120, 120, 80, 30);

  // Smaller background button positioned directly underneath `dialog_button`.
  ui::AXNodeData occluded_tiny_button;
  occluded_tiny_button.id = 4;
  occluded_tiny_button.role = ax::mojom::Role::kButton;
  occluded_tiny_button.SetNameChecked("Occluded Tiny Button");
  occluded_tiny_button.relative_bounds.bounds = gfx::RectF(260, 260, 10, 10);

  ui::AXNodeData dialog;
  dialog.id = 5;
  dialog.role = ax::mojom::Role::kDialog;
  dialog.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  dialog.relative_bounds.bounds = gfx::RectF(100, 100, 400, 300);
  dialog.child_ids = {6};

  ui::AXNodeData dialog_button;
  dialog_button.id = 6;
  dialog_button.role = ax::mojom::Role::kButton;
  dialog_button.SetNameChecked("Confirm");
  dialog_button.relative_bounds.bounds = gfx::RectF(250, 250, 100, 40);

  // Ignored modal dialog positioned over `outside_button` must not swallow
  // hits.
  ui::AXNodeData ignored_modal;
  ignored_modal.id = 7;
  ignored_modal.role = ax::mojom::Role::kAlertDialog;
  ignored_modal.AddState(ax::mojom::State::kIgnored);
  ignored_modal.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  ignored_modal.relative_bounds.bounds = gfx::RectF(0, 0, 100, 100);

  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root,
                  outside_button,
                  occluded_backdrop_button,
                  occluded_tiny_button,
                  dialog,
                  dialog_button,
                  ignored_modal};
  SetMainFrameAXTree(std::move(update));

  // Clicking empty dialog surface must not hit `occluded_backdrop_button`.
  EXPECT_EQ(ResolveAt(140.0f, 130.0f), std::nullopt);
  // Clicking `dialog_button` must not be stolen by smaller
  // `occluded_tiny_button`.
  EXPECT_EQ(ResolveAt(265.0f, 265.0f), "Confirm");
  // Controls outside the active dialog bounds remain reachable even under an
  // ignored modal node.
  EXPECT_EQ(ResolveAt(20.0f, 20.0f), "Outside Action");
}

TEST_F(AxClickTargetResolverTest, SkipsPartiallyOffscreenOversizedContainer) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2};

  // Delegated clickable container scrolled mostly above the viewport: its
  // clipped viewport intersection (`800x200`) is under half the viewport area,
  // whereas its true unclipped layout size (`800x700`) exceeds both container
  // thresholds.
  ui::AXNodeData scrolled_container;
  scrolled_container.id = 2;
  scrolled_container.role = ax::mojom::Role::kGenericContainer;
  scrolled_container.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  scrolled_container.SetNameChecked("Scrolled Wrapper");
  scrolled_container.relative_bounds.offset_container_id = 1;
  scrolled_container.relative_bounds.bounds = gfx::RectF(0, -500, 800, 700);
  scrolled_container.child_ids = {3};

  ui::AXNodeData plain_text;
  plain_text.id = 3;
  plain_text.role = ax::mojom::Role::kStaticText;
  plain_text.SetNameChecked("Visible line");
  plain_text.relative_bounds.offset_container_id = 2;
  plain_text.relative_bounds.bounds = gfx::RectF(10, 520, 200, 20);

  ui::AXTreeUpdate scrolled_update;
  scrolled_update.root_id = 1;
  scrolled_update.nodes = {root, scrolled_container, plain_text};
  SetMainFrameAXTree(std::move(scrolled_update));

  EXPECT_EQ(ResolveAt(20.0f, 25.0f), std::nullopt);
}

TEST_F(AxClickTargetResolverTest, BoundsTraversalAtMaxVisitedNodes) {
  // Build a flat DOM wider than `kMaxVisitedNodes` (10,000). Because children
  // are traversed in reverse DOM order, the last child is visited within the
  // budget while the first child beyond the budget is skipped cleanly.
  constexpr int32_t kFillerNodeCount = 10005;
  ui::AXNodeData wide_root;
  wide_root.id = 1;
  wide_root.role = ax::mojom::Role::kRootWebArea;
  wide_root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  std::vector<ui::AXNodeData> nodes;
  nodes.reserve(kFillerNodeCount + 3);
  nodes.push_back(wide_root);

  ui::AXNodeData beyond_budget_button;
  beyond_budget_button.id = 2;
  beyond_budget_button.role = ax::mojom::Role::kButton;
  beyond_budget_button.SetNameChecked("Beyond Budget");
  beyond_budget_button.relative_bounds.bounds = gfx::RectF(10, 10, 80, 30);
  nodes[0].child_ids.push_back(beyond_budget_button.id);
  nodes.push_back(beyond_budget_button);

  for (int32_t i = 0; i < kFillerNodeCount; ++i) {
    ui::AXNodeData filler;
    filler.id = 3 + i;
    filler.role = ax::mojom::Role::kGenericContainer;
    filler.AddState(ax::mojom::State::kIgnored);
    filler.relative_bounds.bounds = gfx::RectF(500, 500, 1, 1);
    nodes[0].child_ids.push_back(filler.id);
    nodes.push_back(filler);
  }

  ui::AXNodeData within_budget_button;
  within_budget_button.id = 3 + kFillerNodeCount;
  within_budget_button.role = ax::mojom::Role::kButton;
  within_budget_button.SetNameChecked("Within Budget");
  within_budget_button.relative_bounds.bounds = gfx::RectF(200, 10, 80, 30);
  nodes[0].child_ids.push_back(within_budget_button.id);
  nodes.push_back(within_budget_button);

  ui::AXTreeUpdate wide_update;
  wide_update.root_id = 1;
  wide_update.nodes = std::move(nodes);
  SetMainFrameAXTree(std::move(wide_update));

  EXPECT_EQ(ResolveAt(210.0f, 20.0f), "Within Budget");
  EXPECT_EQ(ResolveAt(20.0f, 20.0f), std::nullopt);
}

TEST_F(
    AxClickTargetResolverTest,
    ResolvesModalDialogUnderIgnoredInvisibleHtmlAndVisibleChildOfHiddenParent) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2, 6};

  // Matches Blink's `<dialog>.showModal()` AXTree shape, where `<html>` is
  // serialized as an `ignored invisible` `kGenericContainer` ancestor of the
  // visible `<dialog>`.
  ui::AXNodeData html_container;
  html_container.id = 2;
  html_container.role = ax::mojom::Role::kGenericContainer;
  html_container.AddState(ax::mojom::State::kIgnored);
  html_container.AddState(ax::mojom::State::kInvisible);
  html_container.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  html_container.child_ids = {3, 4};

  ui::AXNodeData inert_background_button;
  inert_background_button.id = 3;
  inert_background_button.role = ax::mojom::Role::kButton;
  inert_background_button.AddState(ax::mojom::State::kInvisible);
  inert_background_button.SetNameChecked("Inert Background");
  inert_background_button.relative_bounds.bounds = gfx::RectF(120, 120, 40, 20);

  ui::AXNodeData modal_dialog;
  modal_dialog.id = 4;
  modal_dialog.role = ax::mojom::Role::kDialog;
  modal_dialog.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  modal_dialog.relative_bounds.bounds = gfx::RectF(100, 100, 400, 300);
  modal_dialog.child_ids = {5};

  ui::AXNodeData dialog_link;
  dialog_link.id = 5;
  dialog_link.role = ax::mojom::Role::kLink;
  dialog_link.SetNameChecked("Link inside the dialog.");
  dialog_link.relative_bounds.bounds = gfx::RectF(120, 120, 160, 30);

  // A `visibility: hidden` container (`State::kInvisible`) with a
  // `visibility: visible` button descendant.
  ui::AXNodeData hidden_container;
  hidden_container.id = 6;
  hidden_container.role = ax::mojom::Role::kGenericContainer;
  hidden_container.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  hidden_container.AddState(ax::mojom::State::kInvisible);
  hidden_container.SetNameChecked("Hidden Container");
  hidden_container.relative_bounds.bounds = gfx::RectF(10, 450, 200, 80);
  hidden_container.child_ids = {7};

  ui::AXNodeData visible_child_button;
  visible_child_button.id = 7;
  visible_child_button.role = ax::mojom::Role::kButton;
  visible_child_button.SetNameChecked("Visible Child");
  visible_child_button.relative_bounds.bounds = gfx::RectF(20, 460, 100, 30);

  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {
      root,        html_container,   inert_background_button, modal_dialog,
      dialog_link, hidden_container, visible_child_button};
  SetMainFrameAXTree(std::move(update));

  EXPECT_EQ(ResolveAt(130.0f, 130.0f), "Link inside the dialog.");
  EXPECT_EQ(ResolveAt(350.0f, 350.0f), std::nullopt);
  EXPECT_EQ(ResolveAt(40.0f, 470.0f), "Visible Child");
  EXPECT_EQ(ResolveAt(160.0f, 470.0f), std::nullopt);
}

TEST_F(AxClickTargetResolverTest,
       ScopesInProcessSubframesAgainstClickableWrappersAndOccludedControls) {
  // Same-site cross-origin subframe (`sub.example.com`) shares `main_rfh()`'s
  // `RenderWidgetHost` when not origin-isolated, exercising in-process
  // cross-origin iframe hit scoping.
  content::RenderFrameHost* cross_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("cross_sub");
  cross_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://sub.example.com/frame"), cross_rfh);
  ASSERT_EQ(cross_rfh->GetRenderWidgetHost(),
            main_rfh()->GetRenderWidgetHost());

  content::RenderFrameHost* same_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("same_sub");
  same_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/frame"), same_rfh);
  ASSERT_EQ(same_rfh->GetRenderWidgetHost(), main_rfh()->GetRenderWidgetHost());

  const ui::AXTreeID main_tree_id = main_rfh()->GetAXTreeID();
  const ui::AXTreeID cross_tree_id = cross_rfh->GetAXTreeID();
  const ui::AXTreeID same_tree_id = same_rfh->GetAXTreeID();

  ui::AXNodeData main_root;
  main_root.id = 1;
  main_root.role = ax::mojom::Role::kRootWebArea;
  main_root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  main_root.child_ids = {2, 3, 5, 6, 7, 8};

  ui::AXNodeData occluded_under_cross;
  occluded_under_cross.id = 2;
  occluded_under_cross.role = ax::mojom::Role::kButton;
  occluded_under_cross.SetNameChecked("Occluded Behind Cross Iframe");
  occluded_under_cross.relative_bounds.bounds = gfx::RectF(110, 110, 40, 20);

  ui::AXNodeData clickable_wrapper;
  clickable_wrapper.id = 3;
  clickable_wrapper.role = ax::mojom::Role::kGenericContainer;
  clickable_wrapper.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  clickable_wrapper.SetNameChecked("Outer Clickable Wrapper");
  clickable_wrapper.relative_bounds.bounds = gfx::RectF(100, 100, 240, 150);
  clickable_wrapper.child_ids = {4};

  ui::AXNodeData cross_iframe;
  cross_iframe.id = 4;
  cross_iframe.role = ax::mojom::Role::kIframe;
  cross_iframe.relative_bounds.bounds = gfx::RectF(100, 100, 200, 120);
  cross_iframe.AddChildTreeId(cross_tree_id);

  ui::AXNodeData occluded_under_same_empty;
  occluded_under_same_empty.id = 5;
  occluded_under_same_empty.role = ax::mojom::Role::kButton;
  occluded_under_same_empty.SetNameChecked("Occluded Behind Same Empty");
  occluded_under_same_empty.relative_bounds.bounds =
      gfx::RectF(410, 110, 40, 20);

  ui::AXNodeData occluded_under_same_button;
  occluded_under_same_button.id = 6;
  occluded_under_same_button.role = ax::mojom::Role::kButton;
  occluded_under_same_button.SetNameChecked("Occluded Tiny Parent Button");
  occluded_under_same_button.relative_bounds.bounds =
      gfx::RectF(420, 180, 10, 10);

  ui::AXNodeData same_iframe;
  same_iframe.id = 7;
  same_iframe.role = ax::mojom::Role::kIframe;
  same_iframe.relative_bounds.bounds = gfx::RectF(400, 100, 250, 180);
  same_iframe.AddChildTreeId(same_tree_id);

  ui::AXNodeData overlay_above_same;
  overlay_above_same.id = 8;
  overlay_above_same.role = ax::mojom::Role::kButton;
  overlay_above_same.SetNameChecked("Floating Overlay");
  overlay_above_same.relative_bounds.bounds = gfx::RectF(580, 110, 50, 30);

  ui::AXTreeUpdate main_update;
  main_update.root_id = 1;
  main_update.nodes = {
      main_root,    occluded_under_cross,      clickable_wrapper,
      cross_iframe, occluded_under_same_empty, occluded_under_same_button,
      same_iframe,  overlay_above_same};
  SetMainFrameAXTree(std::move(main_update));

  ui::AXNodeData cross_root;
  cross_root.id = 1;
  cross_root.role = ax::mojom::Role::kRootWebArea;
  cross_root.relative_bounds.bounds = gfx::RectF(0, 0, 200, 120);
  cross_root.child_ids = {2};

  ui::AXNodeData cross_button;
  cross_button.id = 2;
  cross_button.role = ax::mojom::Role::kButton;
  cross_button.SetNameChecked("Cross Secret");
  cross_button.relative_bounds.bounds = gfx::RectF(5, 5, 100, 40);

  ui::AXTreeUpdate cross_update;
  cross_update.has_tree_data = true;
  cross_update.tree_data.tree_id = cross_tree_id;
  cross_update.tree_data.parent_tree_id = main_tree_id;
  cross_update.root_id = 1;
  cross_update.nodes = {cross_root, cross_button};
  SetAXTreeForTreeId(cross_tree_id, std::move(cross_update));

  ui::AXNodeData same_root;
  same_root.id = 1;
  same_root.role = ax::mojom::Role::kRootWebArea;
  same_root.relative_bounds.bounds = gfx::RectF(0, 0, 250, 180);
  same_root.child_ids = {2};

  ui::AXNodeData same_button;
  same_button.id = 2;
  same_button.role = ax::mojom::Role::kButton;
  same_button.SetNameChecked("Same Origin Button");
  same_button.relative_bounds.bounds = gfx::RectF(10, 70, 150, 60);

  ui::AXTreeUpdate same_update;
  same_update.has_tree_data = true;
  same_update.tree_data.tree_id = same_tree_id;
  same_update.tree_data.parent_tree_id = main_tree_id;
  same_update.root_id = 1;
  same_update.nodes = {same_root, same_button};
  SetAXTreeForTreeId(same_tree_id, std::move(same_update));

  // Click inside in-process cross-origin `cross_iframe` must not leak
  // `"Cross Secret"`, walk up to `"Outer Clickable Wrapper"`, or hit
  // `"Occluded Behind Cross Iframe"`.
  EXPECT_EQ(ResolveAt(120.0f, 115.0f), std::nullopt);
  // Click on `clickable_wrapper` outside `cross_iframe` still resolves.
  EXPECT_EQ(ResolveAt(320.0f, 115.0f), "Outer Clickable Wrapper");
  // Empty space in `same_iframe` must not hit `"Occluded Behind Same Empty"`.
  EXPECT_EQ(ResolveAt(420.0f, 115.0f), std::nullopt);
  // Large button in `same_iframe` must not be stolen by smaller
  // `"Occluded Tiny Parent Button"`.
  EXPECT_EQ(ResolveAt(425.0f, 185.0f), "Same Origin Button");
  // Later-in-DOM overlay button above `same_iframe` remains reachable.
  EXPECT_EQ(ResolveAt(595.0f, 120.0f), "Floating Overlay");
}

TEST_F(AxClickTargetResolverTest,
       FallsBackToTextChildLabelOnlyForNonTextFields) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2, 4};

  ui::AXNodeData unnamed_chip;
  unnamed_chip.id = 2;
  unnamed_chip.role = ax::mojom::Role::kGenericContainer;
  unnamed_chip.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  unnamed_chip.relative_bounds.bounds = gfx::RectF(10, 10, 100, 36);
  unnamed_chip.child_ids = {3};

  ui::AXNodeData chip_label;
  chip_label.id = 3;
  chip_label.role = ax::mojom::Role::kStaticText;
  chip_label.SetNameChecked("Open");
  chip_label.relative_bounds.bounds = gfx::RectF(20, 18, 50, 20);

  ui::AXNodeData unnamed_text_field;
  unnamed_text_field.id = 4;
  unnamed_text_field.role = ax::mojom::Role::kTextField;
  unnamed_text_field.AddState(ax::mojom::State::kEditable);
  unnamed_text_field.relative_bounds.bounds = gfx::RectF(10, 80, 200, 36);
  unnamed_text_field.child_ids = {5};

  ui::AXNodeData typed_value_text;
  typed_value_text.id = 5;
  typed_value_text.role = ax::mojom::Role::kStaticText;
  typed_value_text.SetNameChecked("Sensitive user draft");
  typed_value_text.relative_bounds.bounds = gfx::RectF(15, 88, 150, 20);

  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root, unnamed_chip, chip_label, unnamed_text_field,
                  typed_value_text};
  SetMainFrameAXTree(std::move(update));

  EXPECT_EQ(ResolveAt(30.0f, 25.0f), "Open");
  EXPECT_EQ(ResolveAt(30.0f, 95.0f), "textField");
}

TEST_F(AxClickTargetResolverTest,
       FiltersLandmarksArticlesScrollBarsAndDisabledControls) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2, 5, 8};

  ui::AXNodeData clickable_card;
  clickable_card.id = 2;
  clickable_card.role = ax::mojom::Role::kGenericContainer;
  clickable_card.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  clickable_card.SetNameChecked("Product Card");
  clickable_card.relative_bounds.bounds = gfx::RectF(10, 10, 300, 180);
  clickable_card.child_ids = {3, 4};

  ui::AXNodeData disabled_button;
  disabled_button.id = 3;
  disabled_button.role = ax::mojom::Role::kButton;
  disabled_button.SetRestriction(ax::mojom::Restriction::kDisabled);
  disabled_button.SetNameChecked("Out of Stock");
  disabled_button.relative_bounds.bounds = gfx::RectF(20, 20, 120, 36);

  ui::AXNodeData scrollbar;
  scrollbar.id = 4;
  scrollbar.role = ax::mojom::Role::kScrollBar;
  scrollbar.SetNameChecked("Card Scrollbar");
  scrollbar.relative_bounds.bounds = gfx::RectF(290, 10, 16, 180);

  ui::AXNodeData clickable_nav;
  clickable_nav.id = 5;
  clickable_nav.role = ax::mojom::Role::kNavigation;
  clickable_nav.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  clickable_nav.SetNameChecked("Primary Navigation");
  clickable_nav.relative_bounds.bounds = gfx::RectF(10, 220, 300, 60);
  clickable_nav.child_ids = {6, 7};

  ui::AXNodeData nav_text;
  nav_text.id = 6;
  nav_text.role = ax::mojom::Role::kStaticText;
  nav_text.SetNameChecked("Section label");
  nav_text.relative_bounds.bounds = gfx::RectF(20, 230, 100, 20);

  ui::AXNodeData nav_link;
  nav_link.id = 7;
  nav_link.role = ax::mojom::Role::kLink;
  nav_link.SetNameChecked("Home");
  nav_link.relative_bounds.bounds = gfx::RectF(140, 230, 80, 20);

  ui::AXNodeData clickable_article;
  clickable_article.id = 8;
  clickable_article.role = ax::mojom::Role::kArticle;
  clickable_article.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  clickable_article.SetNameChecked("Blog Post");
  clickable_article.relative_bounds.bounds = gfx::RectF(10, 300, 300, 80);
  clickable_article.child_ids = {9};

  ui::AXNodeData article_text;
  article_text.id = 9;
  article_text.role = ax::mojom::Role::kStaticText;
  article_text.SetNameChecked("Article paragraph");
  article_text.relative_bounds.bounds = gfx::RectF(20, 315, 200, 20);

  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes = {root,      clickable_card,    disabled_button,
                  scrollbar, clickable_nav,     nav_text,
                  nav_link,  clickable_article, article_text};
  SetMainFrameAXTree(std::move(update));

  // Disabled controls and scrollbars return `std::nullopt` without bubbling up
  // to `clickable_card`, while clicking the card background still resolves.
  EXPECT_EQ(ResolveAt(40.0f, 35.0f), std::nullopt);
  EXPECT_EQ(ResolveAt(298.0f, 50.0f), std::nullopt);
  EXPECT_EQ(ResolveAt(180.0f, 100.0f), "Product Card");

  // Landmarks and `kArticle` with delegated `kClick` are skipped unless an
  // inner control is hit.
  EXPECT_EQ(ResolveAt(40.0f, 240.0f), std::nullopt);
  EXPECT_EQ(ResolveAt(160.0f, 240.0f), "Home");
  EXPECT_EQ(ResolveAt(40.0f, 325.0f), std::nullopt);
}

}  // namespace
}  // namespace context_hub
