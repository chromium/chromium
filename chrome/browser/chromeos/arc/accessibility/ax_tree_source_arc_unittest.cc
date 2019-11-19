// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include "base/stl_util.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace arc {

using AXActionType = mojom::AccessibilityActionType;
using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXCollectionInfoData = mojom::AccessibilityCollectionInfoData;
using AXCollectionItemInfoData = mojom::AccessibilityCollectionItemInfoData;
using AXEventData = mojom::AccessibilityEventData;
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

void SetProperty(AXNodeInfoData* node, AXBooleanProperty prop, bool value) {
  if (!node->boolean_properties) {
    node->boolean_properties = base::flat_map<AXBooleanProperty, bool>();
  }
  auto& prop_map = node->boolean_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node,
                 AXStringProperty prop,
                 const std::string& value) {
  if (!node->string_properties) {
    node->string_properties = base::flat_map<AXStringProperty, std::string>();
  }
  auto& prop_map = node->string_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node, AXIntProperty prop, int32_t value) {
  if (!node->int_properties) {
    node->int_properties = base::flat_map<AXIntProperty, int>();
  }
  auto& prop_map = node->int_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowStringProperty prop,
                 const std::string& value) {
  if (!window->string_properties) {
    window->string_properties =
        base::flat_map<AXWindowStringProperty, std::string>();
  }
  auto& prop_map = window->string_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node,
                 AXIntListProperty prop,
                 const std::vector<int>& value) {
  if (!node->int_list_properties) {
    node->int_list_properties =
        base::flat_map<AXIntListProperty, std::vector<int>>();
  }
  auto& prop_map = node->int_list_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowIntListProperty prop,
                 const std::vector<int>& value) {
  if (!window->int_list_properties) {
    window->int_list_properties =
        base::flat_map<AXWindowIntListProperty, std::vector<int>>();
  }
  auto& prop_map = window->int_list_properties.value();
  base::EraseIf(prop_map, [prop](auto it) { return it.first == prop; });
  prop_map.insert(std::make_pair(prop, value));
}

class MockAutomationEventRouter
    : public extensions::AutomationEventRouterInterface {
 public:
  MockAutomationEventRouter() {}
  ~MockAutomationEventRouter() override = default;

  ui::AXTree* tree() { return &tree_; }

  void DispatchAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events) override {
    for (auto&& event : events.events)
      event_count_[event.event_type]++;

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

  std::map<ax::mojom::Event, int> event_count_;
  ui::AXTree tree_;
};

class AXTreeSourceArcTest : public testing::Test,
                            public AXTreeSourceArc::Delegate {
 public:
  class TestAXTreeSourceArc : public AXTreeSourceArc {
   public:
    TestAXTreeSourceArc(AXTreeSourceArc::Delegate* delegate,
                        MockAutomationEventRouter* router)
        : AXTreeSourceArc(delegate), router_(router) {}

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

  void CallGetChildren(
      AXNodeInfoData* node,
      std::vector<AccessibilityInfoDataWrapper*>* out_children) const {
    AccessibilityInfoDataWrapper* node_data = tree_source_->GetFromId(node->id);
    tree_source_->GetChildren(node_data, out_children);
  }

  void CallSerializeNode(AXNodeInfoData* node,
                         std::unique_ptr<ui::AXNodeData>* out_data) const {
    ASSERT_TRUE(out_data);
    AccessibilityInfoDataWrapper* node_data = tree_source_->GetFromId(node->id);
    *out_data = std::make_unique<ui::AXNodeData>();
    tree_source_->SerializeNode(node_data, out_data->get());
  }

  void CallSerializeWindow(AXWindowInfoData* window,
                           std::unique_ptr<ui::AXNodeData>* out_data) const {
    ASSERT_TRUE(out_data);
    AccessibilityInfoDataWrapper* window_data =
        tree_source_->GetFromId(window->window_id);
    *out_data = std::make_unique<ui::AXNodeData>();
    tree_source_->SerializeNode(window_data, out_data->get());
  }

  AccessibilityInfoDataWrapper* CallGetFromId(int32_t id) const {
    return tree_source_->GetFromId(id);
  }

  bool CallGetTreeData(ui::AXTreeData* data) {
    return tree_source_->GetTreeData(data);
  }

  MockAutomationEventRouter* GetRouter() const { return router_.get(); }

  int GetDispatchedEventCount(ax::mojom::Event type) {
    return router_->event_count_[type];
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

 private:
  void OnAction(const ui::AXActionData& data) const override {}

  const std::unique_ptr<MockAutomationEventRouter> router_;
  const std::unique_ptr<AXTreeSourceArc> tree_source_;

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

  // Add another child button.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button2 = event->node_data.back().get();
  button2->id = 2;
  SetProperty(button2, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(button2, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(button2, AXBooleanProperty::IMPORTANCE, true);

  // Non-overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(0, 0, 50, 50);

  // Trigger an update which refreshes the computed bounds used for reordering.
  CallNotifyAccessibilityEvent(event.get());
  std::vector<AccessibilityInfoDataWrapper*> top_to_bottom;
  CallGetChildren(root, &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->GetId());
  EXPECT_EQ(1, top_to_bottom[1]->GetId());

  // Non-overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  CallGetChildren(event->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->GetId());
  EXPECT_EQ(2, top_to_bottom[1]->GetId());

  // Overlapping; right to left.
  button1->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<AccessibilityInfoDataWrapper*> left_to_right;
  CallGetChildren(root, &left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(2, left_to_right[0]->GetId());
  EXPECT_EQ(1, left_to_right[1]->GetId());

  // Overlapping; left to right.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  CallNotifyAccessibilityEvent(event.get());
  left_to_right.clear();
  CallGetChildren(event->node_data[0].get(), &left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(1, left_to_right[0]->GetId());
  EXPECT_EQ(2, left_to_right[1]->GetId());

  // Overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  CallGetChildren(event->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->GetId());
  EXPECT_EQ(1, top_to_bottom[1]->GetId());

  // Overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  CallGetChildren(event->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->GetId());
  EXPECT_EQ(2, top_to_bottom[1]->GetId());

  // Identical. smaller to larger.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<AccessibilityInfoDataWrapper*> dimension;
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->GetId());
  EXPECT_EQ(1, dimension[1]->GetId());

  button1->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->GetId());
  EXPECT_EQ(1, dimension[1]->GetId());

  // Identical. Larger to smaller.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->GetId());
  EXPECT_EQ(2, dimension[1]->GetId());

  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->GetId());
  EXPECT_EQ(2, dimension[1]->GetId());

  EXPECT_EQ(10, GetDispatchedEventCount(ax::mojom::Event::kFocus));

  // Sanity check tree output.
  ExpectTree(
      "id=100 window (0, 0)-(0, 0) child_ids=10\n"
      "  id=10 genericContainer INVISIBLE (0, 0)-(0, 0) restriction=disabled "
      "modal=true child_ids=1,2\n"
      "    id=1 button FOCUSABLE (100, 100)-(100, 100) restriction=disabled "
      "class_name=android.widget.Button\n"
      "    id=2 button FOCUSABLE (100, 100)-(10, 100) restriction=disabled "
      "class_name=android.widget.Button\n");
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputation) {
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
  SetProperty(root, AXStringProperty::CLASS_NAME, "");
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Add child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child1 = event->node_data.back().get();
  child1->id = 1;

  // Add another child.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child2 = event->node_data.back().get();
  child2->id = 2;

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event.get());

  // No attributes.
  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(root, &data);
  std::string name;
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (empty).
  SetProperty(root, AXStringProperty::TEXT, "");

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  // With crrev/1786363, empty text on node will not set the name.
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (non-empty).
  SetProperty(root, AXStringProperty::TEXT, "label text");

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (empty), text (non-empty).
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION, "");

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (non-empty), text (empty).
  SetProperty(root, AXStringProperty::TEXT, "");
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION,
              "label content description");

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label content description", name);

  // Content description (non-empty), text (non-empty).
  SetProperty(root, AXStringProperty::TEXT, "label text");

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label content description label text", name);

  // Name from contents.

  // Root node has no name, but has descendants with name.
  root->string_properties->clear();
  // Name from contents only happens if a node is clickable.
  SetProperty(root, AXBooleanProperty::CLICKABLE, true);
  SetProperty(child1, AXStringProperty::TEXT, "child1 label text");
  SetProperty(child2, AXStringProperty::TEXT, "child2 label text");

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child1 label text child2 label text", name);

  // If a child is also clickable, do not use child property.
  SetProperty(child1, AXBooleanProperty::CLICKABLE, true);
  SetProperty(child2, AXBooleanProperty::CLICKABLE, true);

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // If the node has a name, it should override the contents.
  child1->boolean_properties->clear();
  child2->boolean_properties->clear();
  SetProperty(root, AXStringProperty::TEXT, "root label text");

  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("root label text", name);

  // The placeholder text on the node, should also be appended to the name.
  SetProperty(child2, AXStringProperty::HINT_TEXT, "child2 hint text");
  CallSerializeNode(child2, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child2 label text child2 hint text", name);

  // Clearing both clickable and name from root, the name should not be
  // populated.
  root->boolean_properties->clear();
  root->string_properties->clear();
  CallNotifyAccessibilityEvent(event.get());
  CallSerializeNode(root, &data);
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationTextField) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 1;

  std::unique_ptr<ui::AXNodeData> data;
  SetProperty(root, AXStringProperty::CLASS_NAME, "");

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event.get());

  // Case for when both text property and content_description is non-empty.
  SetProperty(root, AXBooleanProperty::EDITABLE, true);
  SetProperty(root, AXStringProperty::TEXT, "foo@example.com");
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION,
              "Type your email here.");

  CallSerializeNode(root, &data);

  std::string prop;
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &prop));
  EXPECT_EQ("Type your email here.", prop);

  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kValue, &prop));
  EXPECT_EQ("foo@example.com", prop);

  // Case for when text property is empty.
  SetProperty(root, AXStringProperty::TEXT, "");
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION,
              "Type your email here.");

  CallSerializeNode(root, &data);

  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &prop));
  EXPECT_EQ("Type your email here.", prop);
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kValue, &prop));

  // Case for when only text property is non-empty.
  SetProperty(root, AXStringProperty::TEXT, "foo@example.com");
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION, "");

  CallSerializeNode(root, &data);

  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &prop));
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kValue, &prop));
  EXPECT_EQ("foo@example.com", prop);

  // Clearing string properties, the name and the value should not be populated.
  root->string_properties->clear();
  CallSerializeNode(root, &data);
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &prop));
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kValue, &prop));
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationWindow) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 1;

  CallNotifyAccessibilityEvent(event.get());

  // Live edit name related attributes.

  // No attributes.
  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeWindow(root, &data);
  std::string name;
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Title attribute
  SetProperty(root, AXWindowStringProperty::TITLE, "window title");
  CallSerializeWindow(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
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

  // Add a child node to the child window as well.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child_node = event->node_data.back().get();
  child_node->id = 4;
  SetProperty(child_node, AXStringProperty::TEXT, "child node text");

  // Add a child window with no children as well.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child2 = event->window_data->back().get();
  child2->window_id = 5;
  SetProperty(child2, AXWindowStringProperty::TITLE, "child2 window title");

  CallNotifyAccessibilityEvent(event.get());
  std::unique_ptr<ui::AXNodeData> data;
  std::string name;

  CallSerializeWindow(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);

  CallSerializeWindow(child, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);

  CallSerializeNode(node, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("node text", name);
  EXPECT_EQ(ax::mojom::Role::kStaticText, data->role);
  EXPECT_TRUE(data->GetBoolAttribute(ax::mojom::BoolAttribute::kModal));

  CallSerializeNode(child_node, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child node text", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);

  CallSerializeWindow(child2, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child2 window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);

  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, StringPropertiesComputations) {
  auto event = AXEventData::New();
  event->source_id = 1;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 1;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 1;

  // Add a child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child = event->node_data.back().get();
  child->id = 2;

  // Set properties to the root.
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));
  SetProperty(root, AXStringProperty::PACKAGE_NAME, "com.android.vending");

  // Set properties to the child.
  SetProperty(child, AXStringProperty::TOOLTIP, "tooltip text");

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event.get());

  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(root, &data);

  std::string prop;
  // Url includes AXTreeId, which is unguessable. Just verifies the prefix.
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kUrl, &prop));
  EXPECT_EQ(0U, prop.find("com.android.vending/"));

  CallSerializeNode(child, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kTooltip, &prop));
  ASSERT_EQ("tooltip text", prop);
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
  std::vector<AccessibilityInfoDataWrapper*> children;
  for (int i = 0; i < num_trees; i++) {
    CallGetChildren(event->node_data.at(i * tree_size).get(), &children);
    ASSERT_EQ(1U, children.size());
    EXPECT_EQ(i * tree_size + 2, children[0]->GetId());
    children.clear();
    CallGetChildren(event->node_data.at(i * tree_size + 1).get(), &children);
    ASSERT_EQ(2U, children.size());
    EXPECT_EQ(i * tree_size + 3, children[0]->GetId());
    EXPECT_EQ(i * tree_size + 4, children[1]->GetId());
    children.clear();
  }
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, GetTreeDataAppliesFocus) {
  auto event = AXEventData::New();
  event->source_id = 5;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 5;
  SetProperty(root, AXWindowIntListProperty::CHILD_WINDOW_IDS, {1});

  // Add a child window.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child = event->window_data->back().get();
  child->window_id = 1;

  CallNotifyAccessibilityEvent(event.get());
  ui::AXTreeData data;

  // Nothing should be focused when there are no nodes.
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(ui::AXNode::kInvalidAXID, data.focus_id);

  // Add a child node.
  root->root_node_id = 2;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 2;

  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(2, data.focus_id);

  EXPECT_EQ(2, GetDispatchedEventCount(ax::mojom::Event::kFocus));
}

TEST_F(AXTreeSourceArcTest, EventTypeForViewSelected) {
  auto event = AXEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_SELECTED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Add child node.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* button1 = event->node_data.back().get();
  button1->id = 1;
  SetProperty(button1, AXBooleanProperty::FOCUSABLE, true);

  // Add another child with range_info.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* button2 = event->node_data.back().get();
  button2->id = 2;
  button2->range_info = AXRangeInfoData::New();
  SetProperty(button2, AXBooleanProperty::FOCUSABLE, true);

  // Without range_info, kSelection event should be emitted. Usually this event
  // is fired from AdapterView.
  SetProperty(button1, AXBooleanProperty::FOCUSED, true);
  SetProperty(button2, AXBooleanProperty::FOCUSED, false);
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kSelection));

  // Set range_info. Should be kValueChanged.
  SetProperty(button1, AXBooleanProperty::FOCUSED, false);
  SetProperty(button2, AXBooleanProperty::FOCUSED, true);
  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kValueChanged));
}

TEST_F(AXTreeSourceArcTest, SerializeAndUnserialize) {
  auto event = AXEventData::New();
  event->source_id = 10;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));

  // An ignored node.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;

  // |node2| is ignored by default because
  // AXBooleanProperty::IMPORTANCE has a default false value.

  CallNotifyAccessibilityEvent(event.get());
  EXPECT_EQ(1, GetDispatchedEventCount(ax::mojom::Event::kFocus));
  ExpectTree(
      "id=100 window (0, 0)-(0, 0) child_ids=10\n"
      "  id=10 genericContainer INVISIBLE (0, 0)-(0, 0) restriction=disabled "
      "modal=true child_ids=1\n"
      "    id=1 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=2\n"
      "      id=2 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled\n");

  EXPECT_EQ(0U, tree()->GetFromId(10)->GetUnignoredChildCount());

  // An unignored node.
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node3 = event->node_data.back().get();
  node3->id = 3;
  SetProperty(node3, AXStringProperty::CONTENT_DESCRIPTION, "some text");
  SetProperty(node3, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(node2, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({3}));

  // |node3| is unignored since it has some text.

  CallNotifyAccessibilityEvent(event.get());
  ExpectTree(
      "id=100 window (0, 0)-(0, 0) child_ids=10\n"
      "  id=10 genericContainer INVISIBLE (0, 0)-(0, 0) restriction=disabled "
      "modal=true child_ids=1\n"
      "    id=1 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=2\n"
      "      id=2 genericContainer IGNORED INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled child_ids=3\n"
      "        id=3 genericContainer INVISIBLE (0, 0)-(0, 0) "
      "restriction=disabled name=some text\n");
  EXPECT_EQ(1U, tree()->GetFromId(10)->GetUnignoredChildCount());
}

TEST_F(AXTreeSourceArcTest, SerializeWebView) {
  auto event = AXEventData::New();
  event->source_id = 10;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->emplace_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 10;

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 10;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);

  // node1 is a webView
  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node1 = event->node_data.back().get();
  node1->id = 1;
  SetProperty(node1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));
  SetProperty(node1, AXStringProperty::CHROME_ROLE, "rootWebArea");

  event->node_data.emplace_back(AXNodeInfoData::New());
  AXNodeInfoData* node2 = event->node_data.back().get();
  node2->id = 2;
  SetProperty(
      node2, AXIntListProperty::STANDARD_ACTION_IDS,
      std::vector<int>({static_cast<int>(AXActionType::NEXT_HTML_ELEMENT),
                        static_cast<int>(AXActionType::FOCUS)}));

  CallNotifyAccessibilityEvent(event.get());

  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(node1, &data);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, data->role);

  // Node inside a WebView is not ignored even if it's not set importance.
  CallSerializeNode(node2, &data);
  ASSERT_FALSE(data->HasState(ax::mojom::State::kIgnored));
}

}  // namespace arc
