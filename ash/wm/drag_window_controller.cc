// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/window_mirror_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The maximum opacity of the drag phantom window.
constexpr float kDragPhantomMaxOpacity = 0.8f;

// Computes an opacity value for |dragged_window| or for a drag window that
// represents |dragged_window| on |root_window|.
float GetDragWindowOpacity(aura::Window* root_window,
                           aura::Window* dragged_window,
                           bool is_touch_dragging) {
  // For touch dragging, the present function should only need to be used for
  // the drag windows; the opacity of the original window can simply be set to 1
  // in the constructor and reverted in the destructor.
  DCHECK(!is_touch_dragging || dragged_window->GetRootWindow() != root_window);
  // For mouse dragging, if the mouse is in |root_window|, then return 1.
  if (!is_touch_dragging && Shell::Get()->cursor_manager()->GetDisplay().id() ==
                                display::Screen::GetScreen()
                                    ->GetDisplayNearestWindow(root_window)
                                    .id()) {
    return 1.f;
  }

  // Return an opacity value based on what fraction of |dragged_window| is
  // contained in |root_window|.
  gfx::RectF dragged_window_bounds(dragged_window->bounds());
  ::wm::TranslateRectToScreen(dragged_window->parent(), &dragged_window_bounds);
  dragged_window_bounds =
      gfx::TransformAboutPivot(dragged_window_bounds.origin(),
                               dragged_window->transform())
          .MapRect(dragged_window_bounds);
  gfx::RectF visible_bounds(root_window->GetBoundsInScreen());
  visible_bounds.Intersect(dragged_window_bounds);
  return kDragPhantomMaxOpacity * visible_bounds.size().GetArea() /
         dragged_window_bounds.size().GetArea();
}

}  // namespace

// This keeps track of the drag window's state. It creates/destroys/updates
// bounds and opacity based on the current bounds.
class DragWindowController::DragWindowDetails {
 public:
  explicit DragWindowDetails(const display::Display& display)
      : root_window_(Shell::GetRootWindowForDisplayId(display.id())) {}
  DragWindowDetails(const DragWindowDetails&) = delete;
  DragWindowDetails& operator=(const DragWindowDetails&) = delete;
  ~DragWindowDetails() = default;

  void Update(aura::Window* original_window,
              bool is_touch_dragging,
              const std::optional<gfx::Rect>& shadow_bounds) {
    const float opacity =
        GetDragWindowOpacity(root_window_, original_window, is_touch_dragging);
    if (opacity == 0.f) {
      shadow_.reset();
      widget_.reset();
      return;
    }

    if (!widget_)
      CreateDragWindow(original_window, shadow_bounds);

    gfx::Rect bounds = original_window->bounds();
    aura::Window* window = widget_->GetNativeWindow();
    aura::Window::ConvertRectToTarget(original_window->parent(),
                                      window->parent(), &bounds);
    window->SetBounds(bounds);
    window->SetTransform(original_window->transform());
    widget_->SetOpacity(opacity);
  }

 private:
  friend class DragWindowController;

  void CreateDragWindow(aura::Window* original_window,
                        const std::optional<gfx::Rect>& shadow_bounds) {
    DCHECK(!widget_);
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.layer_type = ui::LAYER_NOT_DRAWN;
    params.name = "DragWindow";
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
    params.parent =
        root_window_->GetChildById(original_window->parent()->GetId());

    widget_ = std::make_unique<views::Widget>();
    widget_->set_focus_on_creation(false);
    widget_->Init(std::move(params));

    // TODO(b/252525521): Change this to WindowPreviewView.
    // WindowPreviewView can show transient children, but currently does not
    // show popups due to performance reasons. WindowPreviewView also needs to
    // be modified so that it can optionally be clipped to the main window's
    // bounds.
    widget_->SetContentsView(std::make_unique<WindowMirrorView>(
        original_window, /*show_non_client_view=*/true, /*sync_bounds=*/true));

    aura::Window* window = widget_->GetNativeWindow();
    window->SetId(kShellWindowId_PhantomWindow);
    window->SetProperty(aura::client::kAnimationsDisabledKey, true);
    gfx::Rect bounds = original_window->bounds();
    ::wm::ConvertRectToScreen(original_window->parent(), &bounds);
    window->SetBounds(bounds);

    if (shadow_bounds) {
      shadow_ = std::make_unique<ui::Shadow>();
      shadow_->Init(::wm::kShadowElevationActiveWindow);
      shadow_->SetContentBounds(*shadow_bounds);
      widget_->GetLayer()->Add(shadow_->layer());
    } else {
      ::wm::SetShadowElevation(window, ::wm::kShadowElevationActiveWindow);
    }

    // Show the widget the setup is done.
    widget_->Show();
  }

  // The root window of |widget_|.
  raw_ptr<aura::Window> root_window_;

  // Contains a WindowMirrorView which is a copy of the original window.
  std::unique_ptr<views::Widget> widget_;

  // Optional custom shadow if one is given.
  std::unique_ptr<ui::Shadow> shadow_;
};

DragWindowController::DragWindowController(
    aura::Window* window,
    bool is_touch_dragging,
    const std::optional<gfx::Rect>& shadow_bounds)
    : window_(window),
      is_touch_dragging_(is_touch_dragging),
      shadow_bounds_(shadow_bounds),
      old_opacity_(window->layer()->GetTargetOpacity()) {
  window->layer()->SetOpacity(1.f);

  DCHECK(drag_windows_.empty());
  display::Screen* screen = display::Screen::GetScreen();
  display::Display current = screen->GetDisplayNearestWindow(window_);
  for (const display::Display& display : screen->GetAllDisplays()) {
    if (current.id() == display.id())
      continue;
    drag_windows_.push_back(std::make_unique<DragWindowDetails>(display));
  }
}

DragWindowController::~DragWindowController() {
  LOG_IF(ERROR, old_opacity_ < 1.0f)
      << "Ended drag and restored window to opacity < 1.0f, which is likely "
         "not intended.";
  window_->layer()->SetOpacity(old_opacity_);
}

void DragWindowController::Update() {
  // For mouse dragging, update the opacity of the original window. For touch
  // dragging, just leave that opacity at 1.
  if (!is_touch_dragging_) {
    window_->layer()->SetOpacity(GetDragWindowOpacity(
        window_->GetRootWindow(), window_, /*is_touch_dragging=*/false));
  }

  for (std::unique_ptr<DragWindowDetails>& details : drag_windows_)
    details->Update(window_, is_touch_dragging_, shadow_bounds_);
}

int DragWindowController::GetDragWindowsCountForTest() const {
  int count = 0;
  for (const std::unique_ptr<DragWindowDetails>& details : drag_windows_) {
    if (details->widget_)
      count++;
  }
  return count;
}

const aura::Window* DragWindowController::GetDragWindowForTest(
    size_t index) const {
  for (const std::unique_ptr<DragWindowDetails>& details : drag_windows_) {
    if (details->widget_) {
      if (index == 0)
        return details->widget_->GetNativeWindow();
      index--;
    }
  }
  return nullptr;
}

const ui::Shadow* DragWindowController::GetDragWindowShadowForTest(
    size_t index) const {
  for (const std::unique_ptr<DragWindowDetails>& details : drag_windows_) {
    if (details->widget_) {
      if (index == 0)
        return details->shadow_.get();
      index--;
    }
  }
  return nullptr;
}

void DragWindowController::RequestLayerPaintForTest() {
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  ui::PaintContext context(list.get(), 1.0f, gfx::Rect(),
                           window_->GetHost()->compositor()->is_pixel_canvas());
  for (auto& details : drag_windows_) {
    std::vector<ui::Layer*> layers;
    layers.push_back(details->widget_->GetLayer());
    while (layers.size()) {
      ui::Layer* layer = layers.back();
      layers.pop_back();
      if (layer->delegate())
        layer->delegate()->OnPaintLayer(context);
      for (ui::Layer* child : layer->children()) {
        layers.push_back(child);
      }
    }
  }
}

}  // namespace ash
