// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_screen_context_controller.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/bind.h"
#include "base/macros.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/layer_type.h"

namespace ash {

namespace {

ui::Layer* FindLayerWithClosure(
    ui::Layer* root,
    const base::RepeatingCallback<bool(ui::Layer*)>& callback) {
  if (callback.Run(root))
    return root;
  for (ui::Layer* child : root->children()) {
    ui::Layer* result = FindLayerWithClosure(child, callback);
    if (result)
      return result;
  }
  return nullptr;
}

}  // namespace

class AssistantScreenContextControllerTest : public AshTestBase {
 protected:
  AssistantScreenContextControllerTest() = default;
  ~AssistantScreenContextControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    controller_ =
        Shell::Get()->assistant_controller()->screen_context_controller();
    DCHECK(controller_);
  }

  ash::AssistantScreenContextController* controller() { return controller_; }

 private:
  AssistantScreenContextController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantScreenContextControllerTest);
};

// Verify that incognito windows are blocked in screenshot.
TEST_F(AssistantScreenContextControllerTest, Screenshot) {
  std::unique_ptr<aura::Window> window1 = CreateToplevelTestWindow(
      gfx::Rect(0, 0, 200, 200), desks_util::GetActiveDeskContainerId());
  std::unique_ptr<aura::Window> window2 = CreateToplevelTestWindow(
      gfx::Rect(30, 30, 100, 100), desks_util::GetActiveDeskContainerId());

  ui::Layer* window1_layer = window1->layer();
  ui::Layer* window2_layer = window2->layer();

  window1->SetProperty(kBlockedForAssistantSnapshotKey, true);

  std::unique_ptr<ui::LayerTreeOwner> layer_owner =
      controller()->CreateLayerForAssistantSnapshotForTest();

  // Test that windows marked as blocked for assistant snapshot is not included.
  EXPECT_FALSE(FindLayerWithClosure(
      layer_owner->root(), base::BindRepeating(
                               [](ui::Layer* layer, ui::Layer* mirror) {
                                 return layer->ContainsMirrorForTest(mirror);
                               },
                               window1_layer)));
  EXPECT_TRUE(FindLayerWithClosure(
      layer_owner->root(), base::BindRepeating(
                               [](ui::Layer* layer, ui::Layer* mirror) {
                                 return layer->ContainsMirrorForTest(mirror);
                               },
                               window2_layer)));

  // Test that a solid black layer is inserted.
  EXPECT_TRUE(FindLayerWithClosure(
      layer_owner->root(), base::BindRepeating([](ui::Layer* layer) {
        return layer->type() == ui::LayerType::LAYER_SOLID_COLOR;
      })));
}

}  // namespace ash
