// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_screen_context_controller_impl.h"

#include <memory>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ui/base/window_properties.h"
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

class AssistantScreenContextControllerTest : public AssistantAshTestBase {
 public:
  AssistantScreenContextControllerTest(
      const AssistantScreenContextControllerTest&) = delete;
  AssistantScreenContextControllerTest& operator=(
      const AssistantScreenContextControllerTest&) = delete;

 protected:
  AssistantScreenContextControllerTest()
      : AssistantAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AssistantScreenContextControllerTest() override = default;

  void SetUp() override {
    AssistantAshTestBase::SetUp();

    controller_ =
        Shell::Get()->assistant_controller()->screen_context_controller();
    DCHECK(controller_);
  }

  AssistantScreenContextControllerImpl* controller() { return controller_; }

  AssistantScreenContextModel* model() { return &controller_->model_; }

  void WaitAndAssertAssistantStructure() {
    base::RunLoop run_loop;
    model()->assistant_structure()->GetValueAsync(base::BindOnce(
        [](base::RunLoop* run_loop,
           const ax::mojom::AssistantStructure& structure) {
          run_loop->Quit();
        },
        &run_loop));

    run_loop.Run();
    EXPECT_TRUE(model()->assistant_structure()->HasValue());
  }

  void WaitAndAssertScreenContext(bool include_assistant_structure) {
    base::RunLoop run_loop;
    controller()->RequestScreenContext(
        include_assistant_structure,
        /*region=*/gfx::Rect(),
        base::BindOnce(
            [](base::RunLoop* run_loop, bool include_assistant_structure,
               ax::mojom::AssistantStructurePtr assistant_structure,
               const std::vector<uint8_t>& screenshot) {
              if (include_assistant_structure)
                EXPECT_TRUE(assistant_structure);
              else
                EXPECT_FALSE(assistant_structure);

              run_loop->Quit();
            },
            &run_loop, include_assistant_structure));

    run_loop.Run();
  }

 private:
  AssistantScreenContextControllerImpl* controller_ = nullptr;
};

// Verify that incognito windows are blocked in screenshot.
TEST_F(AssistantScreenContextControllerTest, Screenshot) {
  std::unique_ptr<aura::Window> window1 = CreateToplevelTestWindow(
      gfx::Rect(0, 0, 200, 200), desks_util::GetActiveDeskContainerId());
  std::unique_ptr<aura::Window> window2 = CreateToplevelTestWindow(
      gfx::Rect(30, 30, 100, 100), desks_util::GetActiveDeskContainerId());

  ui::Layer* window1_layer = window1->layer();
  ui::Layer* window2_layer = window2->layer();

  window1->SetProperty(chromeos::kBlockedForAssistantSnapshotKey, true);

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

// Verify that, in Clamshell mode, opening Launcher will request and cache the
// Assistant structure.
TEST_F(AssistantScreenContextControllerTest,
       ShouldCacheAssistantStructureFutureByOpeningLauncher) {
  OpenLauncher();
  WaitAndAssertAssistantStructure();
}

// Verify that, in Clamshell mode, opening Launcher with Assistant UI will
// request and cache the Assistant structure.
TEST_F(AssistantScreenContextControllerTest,
       ShouldCacheAssistantStructureFutureByOpeningLauncherWithAssistantUi) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);
  WaitAndAssertAssistantStructure();
}

// Verify that, in Clamshell mode, closing Launcher will clear the cached
// Assistant structure.
TEST_F(AssistantScreenContextControllerTest,
       ShouldClearAssistantStructureFutureByClosinggLauncher) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);
  WaitAndAssertAssistantStructure();

  CloseLauncher();
  EXPECT_FALSE(model()->assistant_structure()->HasValue());
}

// Verify that, in Clamshell mode, closing Assistant UI will not clear the
// Assistant structure.
TEST_F(AssistantScreenContextControllerTest,
       ShouldNotClearAssistantStructureFutureByClosingAssistantUi) {
  OpenLauncher();
  WaitAndAssertAssistantStructure();

  // Open Assistant UI will not change the cache of Assistant structure.
  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchResult);
  EXPECT_TRUE(model()->assistant_structure()->HasValue());

  // Close Assistant UI will not change the cache of Assistant structure.
  CloseAssistantUi(AssistantExitPoint::kBackInLauncher);
  EXPECT_TRUE(model()->assistant_structure()->HasValue());
}

// Verify that, in Tablet mode, opening Assistant UI will request and cache the
// Assistant structure.
TEST_F(AssistantScreenContextControllerTest,
       ShouldCacheAssistantStructureFutureByOpeningAssistantUiInTabletMode) {
  SetTabletMode(/*enable=*/true);
  ShowAssistantUi(AssistantEntryPoint::kUnspecified);
  WaitAndAssertAssistantStructure();
}

// Verify that, in Tablet mode, closing Assistant UI will clear the Assistant
// structure.
TEST_F(AssistantScreenContextControllerTest,
       ShouldClearAssistantStructureFutureByClosingAssistantUiInTabletMode) {
  SetTabletMode(/*enable=*/true);
  ShowAssistantUi(AssistantEntryPoint::kUnspecified);
  WaitAndAssertAssistantStructure();

  CloseAssistantUi();
  EXPECT_FALSE(model()->assistant_structure()->HasValue());
}

// Verify that screen context request without Assistant structure is returned.
TEST_F(AssistantScreenContextControllerTest,
       ShouldCallCallbackForRequestScreenContextWithoutAssistantStructure) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);
  WaitAndAssertScreenContext(/*include_assistant_structure=*/false);
}

// Verify that screen context request with non-cached Assistant structure is
// returned.
TEST_F(
    AssistantScreenContextControllerTest,
    ShouldCallCallbackForRequestScreenContextWithNonCachedAssistantStructure) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);
  WaitAndAssertScreenContext(/*include_assistant_structure=*/true);
}

// Verify that screen context request with cached Assistant structure is
// returned.
TEST_F(AssistantScreenContextControllerTest,
       ShouldCallCallbackForRequestScreenContextWithCachedAssistantStructure) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);
  WaitAndAssertAssistantStructure();
  WaitAndAssertScreenContext(/*include_assistant_structure=*/true);
}

}  // namespace ash
