// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// A simple tool instance that exposes some additional data for testing.
class TestTool : public PaletteTool {
 public:
  TestTool(Delegate* delegate, PaletteGroup group, PaletteToolId tool_id)
      : PaletteTool(delegate), group_(group), tool_id_(tool_id) {}

  TestTool(const TestTool&) = delete;
  TestTool& operator=(const TestTool&) = delete;

  // PaletteTool:
  PaletteGroup GetGroup() const override { return group_; }
  PaletteToolId GetToolId() const override { return tool_id_; }

  // Shadows the parent declaration since PaletteTool::enabled is not virtual.
  bool enabled() const { return PaletteTool::enabled(); }

 private:
  // PaletteTool:
  views::View* CreateView() override { NOTREACHED(); }
  void OnViewDestroyed() override { FAIL(); }

  PaletteGroup group_;
  PaletteToolId tool_id_;
};

// Base class for tool manager unittests.
class PaletteToolManagerTest : public ::testing::Test,
                               public PaletteToolManager::Delegate,
                               public PaletteTool::Delegate {
 public:
  PaletteToolManagerTest()
      : palette_tool_manager_(new PaletteToolManager(this)) {}

  PaletteToolManagerTest(const PaletteToolManagerTest&) = delete;
  PaletteToolManagerTest& operator=(const PaletteToolManagerTest&) = delete;

  ~PaletteToolManagerTest() override = default;

 protected:
  // PaletteToolManager::Delegate:
  void HidePalette() override {}
  void HidePaletteImmediately() override {}
  void OnActiveToolChanged() override { ++tool_changed_count_; }
  aura::Window* GetWindow() override { NOTREACHED(); }

  // PaletteTool::Delegate:
  void EnableTool(PaletteToolId tool_id) override {}
  void DisableTool(PaletteToolId tool_id) override {}

  // Helper method for returning an unowned pointer to the constructed tool
  // while also adding it to the PaletteToolManager.
  TestTool* BuildTool(PaletteGroup group, PaletteToolId tool_id) {
    auto* tool = new TestTool(this, group, tool_id);
    palette_tool_manager_->AddTool(base::WrapUnique(tool));
    return tool;
  }

  int tool_changed_count_ = 0;
  std::unique_ptr<PaletteToolManager> palette_tool_manager_;
};

}  // namespace

// Verifies that tools can be enabled/disabled and that enabling a tool disables
// only active tools in the same group.
TEST_F(PaletteToolManagerTest, MultipleToolsActivateDeactivate) {
  // Register actions/modes.
  TestTool* action_1 =
      BuildTool(PaletteGroup::ACTION, PaletteToolId::CREATE_NOTE);
  TestTool* action_2 =
      BuildTool(PaletteGroup::ACTION, PaletteToolId::ENTER_CAPTURE_MODE);
  TestTool* mode_1 = BuildTool(PaletteGroup::MODE, PaletteToolId::MAGNIFY);

  EXPECT_FALSE(palette_tool_manager_->HasTool(PaletteToolId::LASER_POINTER));
  TestTool* mode_2 =
      BuildTool(PaletteGroup::MODE, PaletteToolId::LASER_POINTER);
  EXPECT_TRUE(palette_tool_manager_->HasTool(PaletteToolId::LASER_POINTER));

  // Enable mode 1.
  EXPECT_EQ(0, tool_changed_count_);
  palette_tool_manager_->ActivateTool(mode_1->GetToolId());
  EXPECT_FALSE(action_1->enabled());
  EXPECT_FALSE(action_2->enabled());
  EXPECT_TRUE(mode_1->enabled());
  EXPECT_FALSE(mode_2->enabled());

  // Turn a single action on/off. Enabling/disabling the tool does not change
  // any other group's state.
  palette_tool_manager_->ActivateTool(action_1->GetToolId());
  EXPECT_TRUE(action_1->enabled());
  EXPECT_FALSE(action_2->enabled());
  EXPECT_TRUE(mode_1->enabled());
  EXPECT_FALSE(mode_2->enabled());
  palette_tool_manager_->DeactivateTool(action_1->GetToolId());
  EXPECT_FALSE(action_1->enabled());
  EXPECT_FALSE(action_2->enabled());
  EXPECT_TRUE(mode_1->enabled());
  EXPECT_FALSE(mode_2->enabled());

  // Activating a tool on will deactivate any other active tools in the same
  // group.
  palette_tool_manager_->ActivateTool(action_1->GetToolId());
  EXPECT_TRUE(action_1->enabled());
  EXPECT_FALSE(action_2->enabled());
  palette_tool_manager_->ActivateTool(action_2->GetToolId());
  EXPECT_FALSE(action_1->enabled());
  EXPECT_TRUE(action_2->enabled());
  palette_tool_manager_->DeactivateTool(action_2->GetToolId());

  // Activating an already active tool will not do anything.
  palette_tool_manager_->ActivateTool(action_1->GetToolId());
  EXPECT_TRUE(action_1->enabled());
  EXPECT_FALSE(action_2->enabled());
  palette_tool_manager_->ActivateTool(action_1->GetToolId());
  EXPECT_TRUE(action_1->enabled());
  EXPECT_FALSE(action_2->enabled());
  palette_tool_manager_->DeactivateTool(action_1->GetToolId());
}

}  // namespace ash
