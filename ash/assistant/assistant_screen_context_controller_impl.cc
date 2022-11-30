// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_screen_context_controller_impl.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/assistant/controller/assistant_screen_context_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/task/thread_pool.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/snapshot/snapshot.h"
#include "ui/snapshot/snapshot_aura.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// When the screenshot's dimensions are smaller than this size, we will stop
// downsampling.
constexpr int kScreenshotMaxWidth = 1366;
constexpr int kScreenshotMaxHeight = 768;

std::vector<uint8_t> DownsampleAndEncodeImage(gfx::Image image) {
  // We'll downsample the screenshot to avoid exceeding max allowed size on
  // Assistant server side if we are taking screenshot from high-res screen.
  std::vector<uint8_t> res;
  gfx::JPEGCodec::Encode(
      SkBitmapOperations::DownsampleByTwoUntilSize(
          image.AsBitmap(), kScreenshotMaxWidth, kScreenshotMaxHeight),
      /*quality=*/100, &res);
  return res;
}

void EncodeScreenshotAndRunCallback(
    AssistantScreenContextController::RequestScreenshotCallback callback,
    std::unique_ptr<ui::LayerTreeOwner> layer_owner,
    gfx::Image image) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&DownsampleAndEncodeImage, std::move(image)),
      std::move(callback));
}

void MirrorChildren(ui::Layer* to_mirror,
                    ui::Layer* mirror,
                    const ::wm::MapLayerFunc& map_func) {
  for (auto* child : to_mirror->children()) {
    ui::LayerOwner* owner = child->owner();
    ui::Layer* child_mirror = owner ? map_func.Run(owner).release() : nullptr;
    if (child_mirror) {
      mirror->Add(child_mirror);
      MirrorChildren(child, child_mirror, map_func);
    }
  }
}

std::unique_ptr<ui::LayerTreeOwner> MirrorLayersWithClosure(
    ui::LayerOwner* root,
    const ::wm::MapLayerFunc& map_func) {
  DCHECK(root->OwnsLayer());
  auto layer = map_func.Run(root);
  if (!layer)
    return nullptr;

  auto mirror = std::make_unique<ui::LayerTreeOwner>(std::move(layer));
  MirrorChildren(root->layer(), mirror->root(), map_func);
  return mirror;
}

std::unique_ptr<ui::LayerTreeOwner> CreateLayerForAssistantSnapshot(
    aura::Window* root_window) {
  using LayerSet = base::flat_set<const ui::Layer*>;
  LayerSet excluded_layers;
  LayerSet blocked_layers;

  aura::Window* overlay_container =
      ash::Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);

  if (overlay_container)
    excluded_layers.insert(overlay_container->layer());

  aura::Window* always_on_top_container = ash::Shell::GetContainer(
      root_window, kShellWindowId_AlwaysOnTopContainer);

  // Ignore windows in always on top container. This will prevent assistant
  // window from being snapshot.
  // TODO(muyuanli): We can add Ash property to indicate specific windows to
  //                 be excluded from snapshot (e.g. assistant window itself).
  if (always_on_top_container)
    excluded_layers.insert(always_on_top_container->layer());

  aura::Window* app_list_container =
      ash::Shell::GetContainer(root_window, kShellWindowId_AppListContainer);
  aura::Window* app_list_tablet_mode_container =
      ash::Shell::GetContainer(root_window, kShellWindowId_HomeScreenContainer);

  // Prevent app list from being snapshot on top of other contents.
  if (app_list_container)
    excluded_layers.insert(app_list_container->layer());
  if (app_list_tablet_mode_container)
    excluded_layers.insert(app_list_tablet_mode_container->layer());

  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  for (aura::Window* window : windows) {
    if (window->GetProperty(chromeos::kBlockedForAssistantSnapshotKey))
      blocked_layers.insert(window->layer());
  }

  return MirrorLayersWithClosure(
      root_window,
      base::BindRepeating(
          [](LayerSet blocked_layers, LayerSet excluded_layers,
             ui::LayerOwner* owner) -> std::unique_ptr<ui::Layer> {
            // Parent layer is excluded meaning that it's pointless to clone
            // current child and all its descendants. This reduces the number
            // of layers to create.
            if (base::Contains(blocked_layers, owner->layer()->parent()))
              return nullptr;

            if (base::Contains(blocked_layers, owner->layer())) {
              // Blocked layers are replaced with solid black layers so that
              // they won't be included in the screenshot but still preserve
              // the window stacking.
              auto layer =
                  std::make_unique<ui::Layer>(ui::LayerType::LAYER_SOLID_COLOR);
              layer->SetBounds(owner->layer()->bounds());
              layer->SetColor(SK_ColorBLACK);
              return layer;
            }

            if (excluded_layers.count(owner->layer()))
              return nullptr;

            return owner->layer()->Mirror();
          },
          std::move(blocked_layers), std::move(excluded_layers)));
}

}  // namespace

AssistantScreenContextControllerImpl::AssistantScreenContextControllerImpl() =
    default;

AssistantScreenContextControllerImpl::~AssistantScreenContextControllerImpl() =
    default;

void AssistantScreenContextControllerImpl::RequestScreenshot(
    const gfx::Rect& rect,
    RequestScreenshotCallback callback) {
  aura::Window* root_window = Shell::Get()->GetRootWindowForNewWindows();

  std::unique_ptr<ui::LayerTreeOwner> layer_owner =
      CreateLayerForAssistantSnapshot(root_window);

  ui::Layer* root_layer = layer_owner->root();

  gfx::Rect source_rect =
      rect.IsEmpty() ? gfx::Rect(root_window->bounds().size()) : rect;

  // The root layer might have a scaling transform applied (if the user has
  // changed the UI scale via Ctrl-Shift-Plus/Minus). Clear the transform so
  // that the snapshot is taken at 1:1 scale relative to screen pixels.
  root_layer->SetTransform(gfx::Transform());
  root_window->layer()->Add(root_layer);
  root_window->layer()->StackAtBottom(root_layer);

  ui::GrabLayerSnapshotAsync(
      root_layer, source_rect,
      base::BindOnce(&EncodeScreenshotAndRunCallback, std::move(callback),
                     std::move(layer_owner)));
}

std::unique_ptr<ui::LayerTreeOwner>
AssistantScreenContextControllerImpl::CreateLayerForAssistantSnapshotForTest() {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  return CreateLayerForAssistantSnapshot(root_window);
}

}  // namespace ash
