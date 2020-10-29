// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include <utility>

#include "base/optional.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace arc {

using AXActionType = mojom::AccessibilityActionType;
using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXCollectionInfoData = mojom::AccessibilityCollectionInfoData;
using AXCollectionItemInfoData = mojom::AccessibilityCollectionItemInfoData;
using AXEventData = mojom::AccessibilityEventData;
using AXEventIntListProperty = mojom::AccessibilityEventIntListProperty;
using AXEventIntProperty = mojom::AccessibilityEventIntProperty;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXRangeInfoData = mojom::AccessibilityRangeInfoData;
using AXStringListProperty = mojom::AccessibilityStringListProperty;
using AXStringProperty = mojom::AccessibilityStringProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;
using AXWindowStringProperty = mojom::AccessibilityWindowStringProperty;

namespace {

void SetProperty(AXNodeInfoData* node, AXBooleanProperty prop, bool value) {
  arc::SetProperty(node->boolean_properties, prop, value);
}

void SetProperty(AXNodeInfoData* node,
                 AXStringProperty prop,
                 const std::string& value) {
  arc::SetProperty(node->string_properties, prop, value);
}

void SetProperty(AXNodeInfoData* node, AXIntProperty prop, int32_t value) {
  arc::SetProperty(node->int_properties, prop, value);
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowStringProperty prop,
                 const std::string& value) {
  arc::SetProperty(window->string_properties, prop, value);
}

void SetProperty(AXNodeInfoData* node,
                 AXIntListProperty prop,
                 const std::vector<int>& value) {
  arc::SetProperty(node->int_list_properties, prop, value);
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowIntListProperty prop,
                 const std::vector<int>& value) {
  arc::SetProperty(window->int_list_properties, prop, value);
}

void SetProperty(AXEventData* event, AXEventIntProperty prop, int32_t value) {
  arc::SetProperty(event->int_properties, prop, value);
}

void SetProperty(AXEventData* event,
                 AXEventIntListProperty prop,
                 const std::vector<int>& value) {
  arc::SetProperty(event->int_list_properties, prop, value);
}

}  // namespace

class MockAutomationEventRouter
    : public extensions::AutomationEventRouterInterface {
 public:
  MockAutomationEventRouter() {}
  ~MockAutomationEventRouter() override = default;

  ui::AXTree* tree() { return &tree_; }

  void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events) override {
    for (auto&& event : events.events) {
      event_count_[event.event_type]++;
      last_event_type_ = event.event_type;
    }

    for (const auto& update : events.updates)
      tree_.Unserialize(update);
  }

  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) override {}

  void DispatchTreeDestroyedEvent(
      ui::AXTreeID tree_id,
      content::BrowserContext* browser_context) override {}

  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override {}

  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const base::Optional<gfx::Rect>& rect) override {}

  ax::mojom::Event last_event_type() const { return last_event_type_; }

  std::map<ax::mojom::Event, int> event_count_;
  ui::AXTree tree_;

 private:
  ax::mojom::Event last_event_type_;
};

class AXTreeSourceArcTest : public testing::Test,
                            public AXTreeSourceArc::Delegate {
 public:
  class TestAXTreeSourceArc : public AXTreeSourceArc {
   public:
    TestAXTreeSourceArc(AXTreeSourceArc::Delegate* delegate,
                        MockAutomationEventRouter* router)
        : AXTreeSourceArc(delegate, 1.0), router_(router) {}

   private:
    extensions::AutomationEventRouterInterface* GetAutomationEventRouter()
        const override {
      return router_;
    }

    MockAutomationEventRouter* const router_;
  };

  AXTreeSourceArcTest()
      : router_(new MockAutomationEventRouter()),
        tree_source_(new TestAXTreeSourceArc(this, router_.get())) {}

 protected:
  void CallNotifyAccessibilityEvent(AXEventData* event_data) {
    tree_source_->NotifyAccessibilityEvent(event_data);
  }

  const std::vector<ui::AXNode*>& GetChildren(int32_t node_id) {
    ui::AXNode* ax_node = tree()->GetFromId(node_id);
    return ax_node->children();
  }

  const ui::AXNodeData& GetSerializedNode(int32_t node_id) {
    ui::AXNode* ax_node = tree()->GetFromId(node_id);
    return ax_node->data();
  }

  const ui::AXNodeData& GetSerializedWindow(int32_t window_id) {
    ui::AXNode* ax_node = tree()->GetFromId(window_id);
    return ax_node->data();
  }

  bool CallGetTreeData(ui::AXTreeData* data) {
    return tree_source_->GetTreeData(data);
  }

  MockAutomationEventRouter* GetRouter() const { return router_.get(); }

  int GetDispatchedEventCount(ax::mojom::Event type) {
    return router_->event_count_[type];
  }

  ax::mojom::Event last_dispatched_event_type() const {
    return router_->last_event_type();
  }

  ui::AXTree* tree() { return router_->tree(); }

  void ExpectTree(const std::string& expected) {
    const std::string& tree_text = tree()->ToString();
    size_t first_new_line = tree_text.find("\n");
    ASSERT_NE(std::string::npos, first_new_line);
    ASSERT_GT(tree_text.size(), ++first_new_line);

    // Omit the first line, which contains an unguessable ax tree id.
    EXPECT_EQ(expected, tree_text.substr(first_new_line));
  }

  void set_full_focus_mode(bool enabled) { full_focus_mode_ = enabled; }

  bool UseFullFocusMode() const override { return full_focus_mode_; }

 private:
  void OnAction(const ui::AXActionData& data) const override {}

  const std::unique_ptr<MockAutomationEventRouter> router_;
  const std::unique_ptr<AXTreeSourceArc> tree_source_;

  bool full_focus_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArcTest);
};

TEST_F(AXTreeSourceArcTest, ReorderChildrenByLayout) {
  auto event = AXEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Add child button.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button1 = event->node_data.back().get();
  button1->id = 1;
  SetProperty(button1, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(button1, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(button1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(button1, AXStringProperty::CONTENT_DESCRIPTION, "button1");

  // Add another child button.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button2 = event->node_data.back().get();
  button2->id = 2;
  SetProperty(button2, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(button2, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(button2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(button2, AXStringProperty::CONTENT_DESCRIPTION, "button2");

  // Non-overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(0, 0, 50, 50);

  // Trigger an update which refreshes the computed bounds used for reordering.
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ui::AXNode*> top_to_bottom;
  top_to_bottom = GetChildren(root->id);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->id());
  EXPECT_EQ(1, top_to_bottom[1]->id());

  // Non-overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->id());
  EXPECT_EQ(2, top_to_bottom[1]->id());

  // Overlapping; right to left.
  button1->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ui::AXNode*> left_to_right;
  left_to_right = GetChildren(root->id);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(2, left_to_right[0]->id());
  EXPECT_EQ(1, left_to_right[1]->id());

  // Overlapping; left to right.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  CallNotifyAccessibilityEvent(event.get());
  left_to_right = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(1, left_to_right[0]->id());
  EXPECT_EQ(2, left_to_right[1]->id());

  // Overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->id());
  EXPECT_EQ(1, top_to_bottom[1]->id());

  // Overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->id());
  EXPECT_EQ(2, top_to_bottom[1]->id());

  // Identical. smaller to larger.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ui::AXNode*> dimension;
  dimension = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->id());
  EXPECT_EQ(1, dimension[1]->id());

  button1->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->id());
  EXPECT_EQ(1, dimension[1]->id());

  // Identical. Larger to smaller.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  CallNotifyAccessibilityEvent(event.get());
  dimension = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->id());
  EXPECT_EQ(2, dimension[1]->id());

  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension = GetChildren(event->node_data[0].get()->id);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->id());
  EXPECT_EQ(2, dimension[1]->id());

  EXPECT_EQ(10, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  // Check completeness of tree output.
  ExpectTree(
      "id=100 window FOCUSABLE (0, 0)-(0, 0) modal=true child_ids=10\n"
      "  id=10 genericContainer INVISIBLE (0, 0)-(0, 0) restriction=disabled "
      "child_ids=1,2\n"
      "    id=1 button FOCUSABLE (100, 100)-(100, 100) name_from=attribute "
      "restriction=disabled class_name=android.widget.Button name=button1\n"
      "    id=2 button FOCUSABLE (100, 100)-(10, 100) name_from=attribute "
      "restriction=disabled class_name=android.widget.Button name=button2\n");
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationWindow) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 10;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 1;
  root->root_node_id = node->id;

  // Live edit name related attributes.

  ui::AXNodeData data;

  // No attributes.
  CallNotifyAccessibilityEvent(event.get());
  data = GetSerializedWindow(root->window_id);
  std::string name;
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Title attribute
  SetProperty(root, AXWindowStringProperty::TITLE, "window title");
  CallNotifyAccessibilityEvent(event.get());
  data = GetSerializedWindow(root->window_id);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);

  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, NotificationWindow) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 10;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 1;
  root->root_node_id = node->id;
  root->window_type = mojom::AccessibilityWindowType::TYPE_APPLICATION;

  ui::AXNodeData data;

  // Properties of normal app window.
  CallNotifyAccessibilityEvent(event.get());
  data = GetSerializedWindow(root->window_id);
  ASSERT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kModal));
  ASSERT_EQ(ax::mojom::Role::kApplication, data.role);

  // Set the tree as notification window.
  event->notification_key = "test.notification.key";

  CallNotifyAccessibilityEvent(event.get());
  data = GetSerializedWindow(root->window_id);
  ASSERT_FALSE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kModal));
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, data.role);
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationWindowWithChildren) {
  auto event = AXEventData::New();
  event->source_id = 3;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 100;
  root->root_node_id = 3;
  SetProperty(root, AXWindowIntListProperty::CHILD_WINDOW_IDS, {2, 5});
  SetProperty(root, AXWindowStringProperty::TITLE, "window title");

  // Add a child window.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child = event->window_data->back().get();
  child->window_id = 2;
  child->root_node_id = 4;
  SetProperty(child, AXWindowStringProperty::TITLE, "child window title");

  // Add a child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 3;
  SetProperty(node, AXStringProperty::TEXT, "node text");
  SetProperty(node, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node, AXBooleanProperty::VISIBLE_TO_USER, true);

  // Add a child node to the child window as well.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child_node = event->node_data.back().get();
  child_node->id = 4;
  SetProperty(child_node, AXStringProperty::TEXT, "child node text");
  SetProperty(child_node, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(child_node, AXBooleanProperty::VISIBLE_TO_USER, true);

  // Add a child window with no children as well.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child2 = event->window_data->back().get();
  child2->window_id = 5;
  SetProperty(child2, AXWindowStringProperty::TITLE, "child2 window title");

  CallNotifyAccessibilityEvent(event.get());
  ui::AXNodeData data;
  std::string name;

  data = GetSerializedWindow(root->window_id);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kModal));

  data = GetSerializedWindow(child->window_id);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);

  data = GetSerializedNode(node->id);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("node text", name);
  EXPECT_EQ(ax::mojom::Role::kStaticText, data.role);
  ASSERT_FALSE(data.IsIgnored());

  data = GetSerializedNode(child_node->id);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child node text", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);
  ASSERT_FALSE(data.IsIgnored());

  data = GetSerializedWindow(child2->window_id);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child2 window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data.role);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, ComplexTreeStructure) {
  int tree_size = 4;
  int num_trees = 3;

  auto event = AXEventData::New();
  event->source_id = 4;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  // Pick large numbers for the IDs so as not to overlap.
  root_window->window_id = 1000;
  SetProperty(root_window, AXWindowIntListProperty::CHILD_WINDOW_IDS,
              {100, 200, 300});

  // Make three non-overlapping trees rooted at the same window. One tree has
  // the source_id of interest. Each subtree has a root window, which has a
  // root node with one child, and that child has two leaf children.
  for (int i = 0; i < num_trees; i++) {
    event->window_data->push_back(AXWindowInfoData::New());
    AXWindowInfoData* child_window = event->window_data->back().get();
    child_window->window_id = (i + 1) * 100;
    child_window->root_node_id = i * tree_size + 1;

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* root = event->node_data.back().get();
    root->id = i * tree_size + 1;
    root->window_id = (i + 1) * 100;
    SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({i * tree_size + 2}));

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child1 = event->node_data.back().get();
    child1->id = i * tree_size + 2;
    SetProperty(child1, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({i * tree_size + 3, i * tree_size + 4}));

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child2 = event->node_data.back().get();
    child2->id = i * tree_size + 3;

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child3 = event->node_data.back().get();
    child3->id = i * tree_size + 4;
  }

  CallNotifyAccessibilityEvent(event.get());

  // Check that each node subtree tree was added, and that it is correct.
  std::vector<ui::AXNode*> children;
  for (int i = 0; i < num_trees; i++) {
    children = GetChildren(event->node_data.at(i * tree_size).get()->id);
    ASSERT_EQ(1U, children.size());
    EXPECT_EQ(i * tree_size + 2, children[0]->id());
    children.clear();
    children = GetChildren(event->node_data.at(i * tree_size + 1).get()->id);
    ASSERT_EQ(2U, children.size());
    EXPECT_EQ(i * tree_size + 3, children[0]->id());
    EXPECT_EQ(i * tree_size + 4, children[1]->id());
    children.clear();
  }
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, GetTreeDataAppliesFocus) {
  auto event = AXEventData::New();
  event->source_id = 5;
  event->task_id = 1;
  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 5;
  SetProperty(root, AXWindowIntListProperty::CHILD_WINDOW_IDS, {1});

  // Add a child window.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child = event->window_data->back().get();
  child->window_id = 1;

  // Add a child node.
  root->root_node_id = 2;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 2;
  SetProperty(node, AXBooleanProperty::FOCUSED, true);

  CallNotifyAccessibilityEvent(event.get());

  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(root->window_id, data.focus_id);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kLayoutComplete));
}

TEST_F(AXTreeSourceArcTest, OnViewSelectedEvent) {
  auto event = AXEventData::New();
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_SELECTED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* list = event->node_data.back().get();
  list->id = 1;
  SetProperty(list, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(list, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(list, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(list, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({2, 3, 4}));

  // Slider.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* slider = event->node_data.back().get();
  slider->id = 2;
  SetProperty(slider, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(slider, AXBooleanProperty::IMPORTANCE, true);
  slider->range_info = AXRangeInfoData::New();

  // Simple list item.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* simple_item = event->node_data.back().get();
  simple_item->id = 3;
  SetProperty(simple_item, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(simple_item, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(simple_item, AXBooleanProperty::VISIBLE_TO_USER, true);
  simple_item->collection_item_info = AXCollectionItemInfoData::New();

  // This node is not focusable.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* wrap_node = event->node_data.back().get();
  wrap_node->id = 4;
  SetProperty(wrap_node, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(wrap_node, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(wrap_node, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({5}));
  wrap_node->collection_item_info = AXCollectionItemInfoData::New();

  // A list item expected to get the focus.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* item = event->node_data.back().get();
  item->id = 5;
  SetProperty(item, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(item, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(item, AXBooleanProperty::VISIBLE_TO_USER, true);

  // A selected event from Slider is kValueChanged.
  event->source_id = slider->id;
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kValueChanged));

  // A selected event from a collection. In Android, these event properties are
  // populated by AdapterView.
  event->source_id = list->id;
  SetProperty(event.get(), AXEventIntProperty::ITEM_COUNT, 3);
  SetProperty(event.get(), AXEventIntProperty::FROM_INDEX, 0);
  SetProperty(event.get(), AXEventIntProperty::CURRENT_ITEM_INDEX, 2);
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(item->id, data.focus_id);

  // A selected event from a collection item.
  event->source_id = simple_item->id;
  event->int_properties->clear();
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(simple_item->id, data.focus_id);

  // An event from an invisible node is dropped.
  SetProperty(simple_item, AXBooleanProperty::VISIBLE_TO_USER, false);
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(2,
            GetDispatchedEventCount(ax::mojom::Event::kFocus));  // not changed

  // A selected event from non collection node is dropped.
  SetProperty(simple_item, AXBooleanProperty::VISIBLE_TO_USER, true);
  event->source_id = item->id;
  event->int_properties->clear();
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(2,
            GetDispatchedEventCount(ax::mojom::Event::kFocus));  // not changed
}

TEST_F(AXTreeSourceArcTest, OnWindowStateChangedEvent) {
  auto event = AXEventData::New();
  event->task_id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  event->window_id = 1;
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;

  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({2, 3}));
  SetProperty(node1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node2, AXStringProperty::TEXT, "sample string node2.");

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;
  SetProperty(node3, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node3, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node3, AXStringProperty::TEXT, "sample string node3.");

  // Focus will be on the first accessible node (node2).
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  event->source_id = root->id;
  CallNotifyAccessibilityEvent(event.get());
  ui::AXTreeData data;

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node2->id, data.focus_id);

  // focus moved to node3 for some reason.
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->source_id = node3->id;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node3->id, data.focus_id);

  // after moved the focus on the window, keep the same focus on
  // WINDOW_STATE_CHANGED event.
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  event->source_id = root->id;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node3->id, data.focus_id);

  // Simulate opening another window in this task.
  // |root_window->window_id| can be the same as the previous one, but
  // |event->window_id| of the event are always different for different window.
  // This is the same as new WINDOW_STATE_CHANGED event, so focus is at the
  // first accessible node (node2).
  event->window_id = 2;
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  event->source_id = node1->id;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node2->id, data.focus_id);

  // Simulate closing the second window and coming back to the first window.
  // The focus back to the last focus node, which is node3.
  event->window_id = 1;
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  event->source_id = root->id;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node3->id, data.focus_id);

  EXPECT_EQ(5, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, OnFocusEvent) {
  auto event = AXEventData::New();
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXBooleanProperty::VISIBLE_TO_USER, true);
  root->collection_info = AXCollectionInfoData::New();
  root->collection_info->row_count = 2;
  root->collection_info->column_count = 1;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node1, AXBooleanProperty::ACCESSIBILITY_FOCUSED, true);
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node1, AXStringProperty::TEXT, "sample string1.");

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node2, AXStringProperty::TEXT, "sample string2.");

  // Chrome should focus to node2, even if node1 has 'focus' in Android.
  event->source_id = node2->id;
  CallNotifyAccessibilityEvent(event.get());

  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node2->id, data.focus_id);

  // Chrome should focus to node1, even if Android sends focus on List.
  event->source_id = root->id;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node1->id, data.focus_id);

  // VIEW_ACCESSIBILITY_FOCUSED event also updates the focus in screen reader
  // mode.
  set_full_focus_mode(true);
  SetProperty(node1, AXBooleanProperty::ACCESSIBILITY_FOCUSED, false);
  SetProperty(node2, AXBooleanProperty::ACCESSIBILITY_FOCUSED, true);
  event->event_type = AXEventType::VIEW_ACCESSIBILITY_FOCUSED;
  event->source_id = node2->id;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node2->id, data.focus_id);

  EXPECT_EQ(3, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, OnDrawerOpened) {
  auto event = AXEventData::New();
  event->source_id = 10;  // root
  event->task_id = 1;
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  event->event_text = std::vector<std::string>({"Navigation"});

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  /* AXTree of this test:
    [10] root (DrawerLayout)
    --[1] node1 (not-importantForAccessibility) hidden node
    --[2] node2 visible node
    ----[3] node3 node with text
  */
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXStringProperty::CLASS_NAME,
              "androidx.drawerlayout.widget.DrawerLayout");

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;
  SetProperty(node3, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node3, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node3, AXStringProperty::TEXT, "sample string.");

  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  std::string name;
  data = GetSerializedNode(node2->id);
  ASSERT_EQ(ax::mojom::Role::kMenu, data.role);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("Navigation", name);

  // Validate that the drawer title is cached.
  event->event_text.reset();
  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  CallNotifyAccessibilityEvent(event.get());

  data.RemoveStringAttribute(ax::mojom::StringAttribute::kName);
  data = GetSerializedNode(node2->id);
  ASSERT_EQ(ax::mojom::Role::kMenu, data.role);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("Navigation", name);
}

TEST_F(AXTreeSourceArcTest, SerializeAndUnserialize) {
  auto event = AXEventData::New();
  event->source_id = 10;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));

  // An ignored node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;

  // |node2| is ignored by default because
  // AXBooleanProperty::IMPORTANCE has a default false value.

  set_full_focus_mode(true);

  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  ExpectTree(
      "id=100 window FOCUSABLE (0, 0)-(0, 0) modal=true child_ids=10\n"
      "  id=10 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=1\n"
      "    id=1 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=2\n"
      "      id=2 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled\n");

  EXPECT_EQ(0U, tree()->GetFromId(10)->GetUnignoredChildCount());

  // An unignored node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;
  SetProperty(node3, AXStringProperty::CONTENT_DESCRIPTION, "some text");
  SetProperty(node3, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));

  // |node3| is unignored since it has some text.

  CallNotifyAccessibilityEvent(event.get());
  ExpectTree(
      "id=100 window FOCUSABLE (0, 0)-(0, 0) modal=true child_ids=10\n"
      "  id=10 genericContainer INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=1\n"
      "    id=1 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=2\n"
      "      id=2 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=3\n"
      "        id=3 genericContainer INVISIBLE (0, 0)-(0, 0) "
      "name_from=attribute restriction=disabled name=some text\n");
  EXPECT_EQ(1U, tree()->GetFromId(10)->GetUnignoredChildCount());
}

TEST_F(AXTreeSourceArcTest, SerializeVirtualNode) {
  auto event = AXEventData::New();
  event->source_id = 10;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  // Add a webview node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* webview = event->node_data.back().get();
  webview->id = 1;
  SetProperty(webview, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(webview, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({2, 3}));
  SetProperty(webview, AXStringProperty::CHROME_ROLE, "rootWebArea");

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button1 = event->node_data.back().get();
  button1->id = 2;
  button1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  button1->is_virtual_node = true;
  SetProperty(button1, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(
      button1, AXIntListProperty::STANDARD_ACTION_IDS,
      std::vector<int>({static_cast<int>(AXActionType::NEXT_HTML_ELEMENT),
                        static_cast<int>(AXActionType::FOCUS)}));
  SetProperty(button1, AXStringProperty::CONTENT_DESCRIPTION, "button1");

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button2 = event->node_data.back().get();
  button2->id = 3;
  button2->bounds_in_screen = gfx::Rect(0, 0, 100, 100);
  button2->is_virtual_node = true;
  SetProperty(button2, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(
      button2, AXIntListProperty::STANDARD_ACTION_IDS,
      std::vector<int>({static_cast<int>(AXActionType::NEXT_HTML_ELEMENT),
                        static_cast<int>(AXActionType::FOCUS)}));
  SetProperty(button2, AXStringProperty::CONTENT_DESCRIPTION, "button2");

  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  data = GetSerializedNode(webview->id);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, data.role);

  // Node inside a WebView is not ignored even if it's not set importance.
  data = GetSerializedNode(button1->id);
  ASSERT_FALSE(data.IsIgnored());

  data = GetSerializedNode(button2->id);
  ASSERT_FALSE(data.IsIgnored());

  // Children are not reordered under WebView.
  std::vector<ui::AXNode*> children;
  children = GetChildren(webview->id);
  ASSERT_EQ(2U, children.size());
  EXPECT_EQ(button1->id, children[0]->id());
  EXPECT_EQ(button2->id, children[1]->id());
}

TEST_F(AXTreeSourceArcTest, SyncFocus) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Add child nodes.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(node1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node1, AXStringProperty::CONTENT_DESCRIPTION, "node1");
  node1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);

  // Add a child node to |node1|, but it's not an important node.
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;

  // Initially |node1| has focus.
  CallNotifyAccessibilityEvent(event.get());
  ui::AXTreeData data;
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node1->id, data.focus_id);

  // Focus event to a non-important node. The descendant important node |node1|
  // gets focus instead.
  event->source_id = node3->id;
  event->event_type = AXEventType::VIEW_FOCUSED;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(node1->id, data.focus_id);

  // When the focused node disappeared from the tree, move the focus to the
  // root.
  root->int_list_properties->clear();
  event->node_data.resize(1);

  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(root_window->window_id, data.focus_id);
}

TEST_F(AXTreeSourceArcTest, LiveRegion) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));
  SetProperty(root, AXIntProperty::LIVE_REGION,
              static_cast<int32_t>(mojom::AccessibilityLiveRegionType::POLITE));

  // Add child nodes.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXStringProperty::TEXT, "text 1");

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(node2, AXStringProperty::TEXT, "text 2");

  CallNotifyAccessibilityEvent(event.get());

  ui::AXNodeData data;
  data = GetSerializedNode(root->id);
  std::string status;
  ASSERT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                      &status));
  ASSERT_EQ(status, "polite");
  for (AXNodeInfoData* node : {root, node1, node2}) {
    data = GetSerializedNode(node->id);
    ASSERT_TRUE(data.GetStringAttribute(
        ax::mojom::StringAttribute::kContainerLiveStatus, &status));
    ASSERT_EQ(status, "polite");
  }

  EXPECT_EQ(0, GetDispatchedEventCount(ax::mojom::Event::kLiveRegionChanged));

  // Modify text of node1.
  SetProperty(node1, AXStringProperty::TEXT, "modified text 1");
  CallNotifyAccessibilityEvent(event.get());

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kLiveRegionChanged));
}

TEST_F(AXTreeSourceArcTest, StateDescriptionChangedEvent) {
  auto event = AXEventData::New();
  event->source_id = 10;
  event->task_id = 1;
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* range_widget = event->node_data.back().get();
  range_widget->range_info = AXRangeInfoData::New();
  range_widget->id = 10;

  // State description changed event from range widget.
  std::vector<int> content_change_types = {
      static_cast<int>(mojom::ContentChangeType::TEXT),
      static_cast<int>(mojom::ContentChangeType::STATE_DESCRIPTION)};
  SetProperty(event.get(), AXEventIntListProperty::CONTENT_CHANGE_TYPES,
              content_change_types);
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(ax::mojom::Event::kValueChanged, last_dispatched_event_type());

  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(ax::mojom::Event::kValueChanged, last_dispatched_event_type());

  // State description changed event from non range widget.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* not_range_widget = event->node_data.back().get();
  not_range_widget->id = 11;

  event->source_id = 11;
  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(ax::mojom::Event::kAriaAttributeChanged,
            last_dispatched_event_type());

  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(ax::mojom::Event::kAriaAttributeChanged,
            last_dispatched_event_type());
}

TEST_F(AXTreeSourceArcTest, EventWithWrongSourceId) {
  auto event = AXEventData::New();
  event->source_id = 99999;  // This doesn't exist in serialized nodes.
  event->task_id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 10;

  // This test only verifies that wrong source id won't make Chrome crash.

  event->event_type = AXEventType::VIEW_FOCUSED;
  CallNotifyAccessibilityEvent(event.get());

  event->event_type = AXEventType::VIEW_SELECTED;
  CallNotifyAccessibilityEvent(event.get());

  event->event_type = AXEventType::WINDOW_STATE_CHANGED;
  event->event_text = std::vector<std::string>({"test text."});
  SetProperty(event.get(), AXEventIntListProperty::CONTENT_CHANGE_TYPES,
              {static_cast<int>(mojom::ContentChangeType::STATE_DESCRIPTION)});
  CallNotifyAccessibilityEvent(event.get());

  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  CallNotifyAccessibilityEvent(event.get());
}

TEST_F(AXTreeSourceArcTest, EnsureNodeIdMapCleared) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 2;
  root_window->root_node_id = 1;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 1;

  event->event_type = AXEventType::VIEW_SELECTED;
  CallNotifyAccessibilityEvent(event.get());

  // Ensures that the first event is dropped while handling it.
  EXPECT_EQ(0, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  EXPECT_EQ(0, GetDispatchedEventCount(ax::mojom::Event::kValueChanged));

  event->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  // Swaps ids of node and root_window.
  event->source_id = 2;
  root_window->window_id = 1;
  root_window->root_node_id = 2;
  node->id = 2;

  // If the previous node id mapping remains, this will enter infinite loop.
  CallNotifyAccessibilityEvent(event.get());
}

TEST_F(AXTreeSourceArcTest, ControlReceivesFocus) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root_node = event->node_data.back().get();
  root_node->id = 10;
  SetProperty(root_node, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1}));

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 1;
  SetProperty(node, AXStringProperty::CLASS_NAME, ui::kAXSeekBarClassname);
  SetProperty(node, AXStringProperty::TEXT, "");
  SetProperty(node, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(node, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(node, AXBooleanProperty::IMPORTANCE, true);

  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  ui::AXNodeData data;
  std::string name;
  data = GetSerializedNode(node->id);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ(ax::mojom::Role::kSlider, data.role);

  ui::AXTreeData tree_data;
  EXPECT_TRUE(CallGetTreeData(&tree_data));
  EXPECT_EQ(node->id, tree_data.focus_id);
}

}  // namespace arc
