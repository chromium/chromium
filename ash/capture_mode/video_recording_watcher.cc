// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/video_recording_watcher.h"

#include <memory>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Returns true if |window_1| and |window_2| are both windows that belong to
// the same Desk. Note that it will return false for windows that don't belong
// to any desk (such as always-on-top windows or PIPs).
bool AreWindowsOnSameDesk(aura::Window* window_1, aura::Window* window_2) {
  auto* container_1 = desks_util::GetDeskContainerForContext(window_1);
  auto* container_2 = desks_util::GetDeskContainerForContext(window_2);
  return container_1 && container_2 && container_1 == container_2;
}

}  // namespace

// -----------------------------------------------------------------------------
// RecordedWindowRootObserver:

// Defines an observer to observe the hierarchy changes of the root window under
// which the recorded window resides. This is only constructed when performing a
// window recording type.
class RecordedWindowRootObserver : public aura::WindowObserver {
 public:
  RecordedWindowRootObserver(aura::Window* root, VideoRecordingWatcher* owner)
      : root_(root), owner_(owner) {
    DCHECK(root_);
    DCHECK(owner_);
    DCHECK(root_->IsRootWindow());
    DCHECK_EQ(owner_->recording_source_, CaptureModeSource::kWindow);

    root_->AddObserver(this);
  }
  RecordedWindowRootObserver(const RecordedWindowRootObserver&) = delete;
  RecordedWindowRootObserver& operator=(const RecordedWindowRootObserver&) =
      delete;
  ~RecordedWindowRootObserver() override { root_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    DCHECK_EQ(params.receiver, root_);
    owner_->OnRootHierarchyChanged(params.target);
  }

  void OnWindowDestroying(aura::Window* window) override {
    // We should never get here, as the recorded window gets moved to a
    // different display before the root of another is destroyed. So this root
    // observer should have been destroyed already.
    NOTREACHED();
  }

 private:
  aura::Window* const root_;
  VideoRecordingWatcher* const owner_;
};

// -----------------------------------------------------------------------------
//  VideoRecordingWatcher:

VideoRecordingWatcher::VideoRecordingWatcher(
    CaptureModeController* controller,
    aura::Window* window_being_recorded)
    : controller_(controller),
      window_being_recorded_(window_being_recorded),
      current_root_(window_being_recorded->GetRootWindow()),
      recording_source_(controller_->source()) {
  DCHECK(controller_);
  DCHECK(window_being_recorded_);
  DCHECK(current_root_);
  DCHECK(controller_->is_recording_in_progress());

  if (!window_being_recorded_->IsRootWindow()) {
    DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);
    non_root_window_capture_request_ =
        window_being_recorded_->MakeWindowCapturable();
    root_observer_ =
        std::make_unique<RecordedWindowRootObserver>(current_root_, this);
    Shell::Get()->activation_client()->AddObserver(this);
  }

  if (recording_source_ == CaptureModeSource::kRegion)
    partial_region_bounds_ = controller_->user_capture_region();

  display::Screen::GetScreen()->AddObserver(this);
  window_being_recorded_->AddObserver(this);
}

VideoRecordingWatcher::~VideoRecordingWatcher() {
  DCHECK(window_being_recorded_);

  if (recording_source_ == CaptureModeSource::kWindow)
    Shell::Get()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  window_being_recorded_->RemoveObserver(this);
}

void VideoRecordingWatcher::OnWindowParentChanged(aura::Window* window,
                                                  aura::Window* parent) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (window == window_being_recorded_)
    UpdateShouldPaintLayer();
}

void VideoRecordingWatcher::OnWindowOpacitySet(
    aura::Window* window,
    ui::PropertyChangeReason reason) {
  if (window == window_being_recorded_)
    UpdateShouldPaintLayer();
}

void VideoRecordingWatcher::OnWindowStackingChanged(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());

  // EndVideoRecording() destroys |this|. No need to remove observer here, since
  // it will be done in the destructor.
  controller_->EndVideoRecording(EndRecordingReason::kDisplayOrWindowClosing);
}

void VideoRecordingWatcher::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);

  // We should never get here, since OnWindowDestroying() calls
  // EndVideoRecording() which deletes us.
  NOTREACHED();
}

void VideoRecordingWatcher::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  root_observer_.reset();
  current_root_ = new_root;

  if (!new_root) {
    // EndVideoRecording() destroys |this|.
    controller_->EndVideoRecording(EndRecordingReason::kDisplayOrWindowClosing);
    return;
  }

  root_observer_ =
      std::make_unique<RecordedWindowRootObserver>(current_root_, this);
  controller_->OnRecordedWindowChangingRoot(window_being_recorded_, new_root);
}

void VideoRecordingWatcher::OnPaintLayer(const ui::PaintContext& context) {
  if (!should_paint_layer_)
    return;

  DCHECK_NE(recording_source_, CaptureModeSource::kFullscreen);

  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();
  auto* color_provider = AshColorProvider::Get();
  const SkColor dimming_color = color_provider->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield40);
  canvas->DrawColor(dimming_color);

  // We don't draw a region border around the recorded window. We just paint the
  // above shield as a backdrop.
  if (recording_source_ == CaptureModeSource::kWindow)
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  gfx::Rect region = gfx::ScaleToEnclosingRect(partial_region_bounds_, dsf);
  region.Inset(-capture_mode::kCaptureRegionBorderStrokePx,
               -capture_mode::kCaptureRegionBorderStrokePx);
  canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);

  // Draw the region border.
  cc::PaintFlags border_flags;
  border_flags.setColor(capture_mode::kRegionBorderColor);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setStrokeWidth(capture_mode::kCaptureRegionBorderStrokePx);
  canvas->DrawRect(gfx::RectF(region), border_flags);
}

void VideoRecordingWatcher::OnWindowActivated(ActivationReason reason,
                                              aura::Window* gained_active,
                                              aura::Window* lost_active) {
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (recording_source_ == CaptureModeSource::kFullscreen)
    return;

  if (!(metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
                   DISPLAY_METRIC_DEVICE_SCALE_FACTOR))) {
    return;
  }

  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(current_root_).id();
  if (display_id != display.id())
    return;

  const auto& root_bounds = current_root_->bounds();
  controller_->PushNewRootSizeToRecordingService(root_bounds.size());

  DCHECK(layer());
  layer()->SetBounds(root_bounds);
}

void VideoRecordingWatcher::OnDimmedWindowDestroying(
    aura::Window* dimmed_window) {
  dimmers_.erase(dimmed_window);
}

void VideoRecordingWatcher::OnDimmedWindowParentChanged(
    aura::Window* dimmed_window) {
  // If the dimmed window moves to another display or a different desk, we no
  // longer dim it.
  if (dimmed_window->GetRootWindow() != current_root_ ||
      !AreWindowsOnSameDesk(dimmed_window, window_being_recorded_)) {
    dimmers_.erase(dimmed_window);
  }
}

bool VideoRecordingWatcher::IsWindowDimmedForTesting(
    aura::Window* window) const {
  return dimmers_.contains(window);
}

void VideoRecordingWatcher::SetLayer(std::unique_ptr<ui::Layer> layer) {
  if (layer) {
    layer->set_delegate(this);
    layer->SetName("Recording Shield");
  }
  LayerOwner::SetLayer(std::move(layer));
  UpdateShouldPaintLayer();
  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnRootHierarchyChanged(aura::Window* target) {
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  if (target != window_being_recorded_ && !dimmers_.contains(target) &&
      CanIncludeWindowInMruList(target)) {
    UpdateLayerStackingAndDimmers();
  }
}

bool VideoRecordingWatcher::CalculateShouldPaintLayer() const {
  if (recording_source_ == CaptureModeSource::kFullscreen)
    return false;

  if (recording_source_ == CaptureModeSource::kRegion)
    return true;

  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);
  return window_being_recorded_->TargetVisibility() &&
         window_being_recorded_->layer()->GetTargetVisibility() &&
         window_being_recorded_->layer()->GetTargetOpacity() > 0;
}

void VideoRecordingWatcher::UpdateShouldPaintLayer() {
  const bool new_value = CalculateShouldPaintLayer();
  if (new_value == should_paint_layer_)
    return;

  should_paint_layer_ = new_value;
  if (!should_paint_layer_) {
    // If we're not painting the shield, we don't need the individual dimmers
    // either.
    dimmers_.clear();
  }

  if (layer())
    layer()->SchedulePaint(layer()->bounds());
}

void VideoRecordingWatcher::UpdateLayerStackingAndDimmers() {
  if (!layer())
    return;

  DCHECK_NE(recording_source_, CaptureModeSource::kFullscreen);

  const bool is_recording_window =
      recording_source_ == CaptureModeSource::kWindow;

  ui::Layer* new_parent =
      is_recording_window
          ? window_being_recorded_->layer()->parent()
          : current_root_->GetChildById(kShellWindowId_OverlayContainer)
                ->layer();
  ui::Layer* old_parent = layer()->parent();
  DCHECK(new_parent || is_recording_window);

  if (!new_parent && old_parent) {
    // If the window gets removed from the hierarchy, we remove the shield layer
    // too, as well as any dimming of windows we have.
    old_parent->Remove(layer());
    dimmers_.clear();
    return;
  }

  if (new_parent != old_parent) {
    // We get here the first time we parent the shield layer under the overlay
    // container when we're recording a partial region, or when recording a
    // window, and it gets moved to another display, or moved to a different
    // desk.
    new_parent->Add(layer());
    layer()->SetBounds(current_root_->bounds());
  }

  // When recording a partial region, the shield layer is stacked at the top of
  // everything in the overlay container.
  if (!is_recording_window) {
    new_parent->StackAtTop(layer());
    return;
  }

  // However, when recording a window, we stack the shield layer below the
  // recorded window's layer. This takes care of dimming any windows below the
  // recorded window in the z-order.
  new_parent->StackBelow(layer(), window_being_recorded_->layer());

  // If the shield is not painted, all the individual dimmers should be removed.
  if (!should_paint_layer_) {
    dimmers_.clear();
    return;
  }

  // For windows that are above the recorded window in the z-order and on the
  // same display, they're dimmed separately.
  const SkColor dimming_color = AshColorProvider::Get()->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield40);
  // We use |kAllDesks| here for the following reasons:
  // 1- A dimmed window can move out from the desk where the window being
  //    recorded is (either by keyboard shortcut or drag and drop in overview).
  // 2- The recorded window itself can move out from the active desk to an
  //    inactive desk that has other windows.
  // In (1), we want to remove the dimmers of those windows that moved out.
  // In (2), we want to remove the dimmers of the window on the active desk, and
  // create ones for the windows in the inactive desk (if any of them is above
  // the recorded window).
  const auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          DesksMruType::kAllDesks);
  bool did_find_recorded_window = false;
  // Note that the order of |mru_windows| are from top-most first.
  for (auto* window : mru_windows) {
    if (window == window_being_recorded_) {
      did_find_recorded_window = true;
      continue;
    }

    // No need to dim windows that are below the window being recorded in
    // z-order, or those on other displays, or other desks.
    if (did_find_recorded_window || window->GetRootWindow() != current_root_ ||
        !AreWindowsOnSameDesk(window, window_being_recorded_)) {
      dimmers_.erase(window);
      continue;
    }

    auto& dimmer = dimmers_[window];
    if (!dimmer) {
      dimmer = std::make_unique<WindowDimmer>(window, /*animate=*/false, this);
      dimmer->SetDimColor(dimming_color);
      dimmer->window()->Show();
    }
  }
}

}  // namespace ash
