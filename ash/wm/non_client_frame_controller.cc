// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/non_client_frame_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/frame_utils.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/ws/window_service_owner.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/window_properties.h"
#include "services/ws/window_service.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::NonClientFrameController*);

namespace ash {
namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(NonClientFrameController*,
                             kNonClientFrameControllerKey,
                             nullptr);

// This class supports draggable app windows that paint their own custom frames.
// It uses empty insets and doesn't paint anything.
class EmptyDraggableNonClientFrameView : public views::NonClientFrameView {
 public:
  EmptyDraggableNonClientFrameView() = default;
  ~EmptyDraggableNonClientFrameView() override = default;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return bounds();
  }
  int NonClientHitTest(const gfx::Point& point) override {
    return FrameBorderNonClientHitTest(this, point);
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyDraggableNonClientFrameView);
};

class WmNativeWidgetAura : public views::NativeWidgetAura {
 public:
  WmNativeWidgetAura(views::internal::NativeWidgetDelegate* delegate,
                     bool remove_standard_frame)
      // The NativeWidget is mirroring the real Widget created in client code.
      // |is_parallel_widget_in_window_manager| is used to indicate this
      : views::NativeWidgetAura(delegate,
                                true /* is_parallel_widget_in_window_manager */,
                                Shell::Get()->aura_env()),
        remove_standard_frame_(remove_standard_frame) {}
  ~WmNativeWidgetAura() override = default;

  void set_cursor(const ui::Cursor& cursor) { cursor_ = cursor; }

  // views::NativeWidgetAura:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    // TODO(sky): investigate why we have this. Seems this should be the same
    // as not specifying client area insets.
    if (remove_standard_frame_) {
      wm::InstallResizeHandleWindowTargeterForWindow(GetNativeWindow());
      return new EmptyDraggableNonClientFrameView();
    }

    // See description for details on ownership.
    auto* custom_frame_view = new NonClientFrameViewAsh(GetWidget());

    // Only the header actually paints any content. So the rest of the region is
    // marked as transparent content (see below in NonClientFrameController()
    // ctor). So, it is necessary to provide a texture-layer for the header
    // view.
    views::View* header_view = custom_frame_view->GetHeaderView();
    if (header_view) {
      header_view->SetPaintToLayer(ui::LAYER_TEXTURED);
      header_view->layer()->SetFillsBoundsOpaquely(false);
    }

    return custom_frame_view;
  }

  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return cursor_;
  }

 private:
  const bool remove_standard_frame_;

  // The cursor for this widget. CompoundEventFilter will retrieve this cursor
  // via GetCursor and update the CursorManager's active cursor as appropriate
  // (i.e. when the mouse pointer is over this widget).
  ui::Cursor cursor_;

  DISALLOW_COPY_AND_ASSIGN(WmNativeWidgetAura);
};

// ContentsViewMus links the ash Widget's accessibility node tree with the one
// inside a remote process app. This is needed to support focus; ash needs to
// have "focus" on a leaf node in its Widget/View hierarchy in order for the
// accessibility subsystem to see focused nodes in the remote app.
class ContentsViewMus : public views::View {
 public:
  ContentsViewMus() = default;
  ~ContentsViewMus() override = default;

  // views::View:
  const char* GetClassName() const override { return "ContentsViewMus"; }
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    std::string* tree_id =
        GetWidget()->GetNativeWindow()->GetProperty(ui::kChildAXTreeID);
    // Property may not be available immediately, but focus is eventually
    // consistent.
    if (!tree_id)
      return;
    node_data->AddStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                                  *tree_id);
    node_data->role = ax::mojom::Role::kClient;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentsViewMus);
};

class ClientViewMus : public views::ClientView {
 public:
  ClientViewMus(views::Widget* widget,
                views::View* contents_view,
                NonClientFrameController* frame_controller)
      : views::ClientView(widget, contents_view),
        frame_controller_(frame_controller) {}
  ~ClientViewMus() override = default;

  // views::ClientView:
  bool CanClose() override {
    // CanClose() is called when the user requests the window to close (such as
    // clicking the close button). As this window is managed by a remote client
    // pass the request to the remote client and return false (to cancel the
    // close). If the remote client wants the window to close, it will close it
    // in a way that does not reenter this code path.
    Shell::Get()->window_service_owner()->window_service()->RequestClose(
        frame_controller_->window());
    return false;
  }

  // views::View:
  const char* GetClassName() const override { return "ClientViewMus"; }

 private:
  NonClientFrameController* frame_controller_;

  DISALLOW_COPY_AND_ASSIGN(ClientViewMus);
};

}  // namespace

NonClientFrameController::NonClientFrameController(
    aura::Window* parent,
    aura::Window* context,
    const gfx::Rect& bounds,
    ws::mojom::WindowType window_type,
    aura::PropertyConverter* property_converter,
    std::map<std::string, std::vector<uint8_t>>* properties)
    : widget_(new views::Widget), window_(nullptr) {
  // To simplify things this code creates a Widget. While a Widget is created
  // we need to ensure we don't inadvertently change random properties of the
  // underlying ui::Window. For example, showing the Widget shouldn't change
  // the bounds of the ui::Window in anyway.
  //
  // Assertions around InitParams::Type matching ws::mojom::WindowType exist in
  // MusClient.
  views::Widget::InitParams params(
      static_cast<views::Widget::InitParams::Type>(window_type));
  DCHECK((parent && !context) || (!parent && context));
  params.parent = parent;
  params.context = context;
  // TODO: properly set |params.activatable|. Should key off whether underlying
  // (mus) window can have focus.
  params.delegate = this;
  params.bounds = bounds;
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  params.layer_type = ui::LAYER_SOLID_COLOR;
  WmNativeWidgetAura* native_widget =
      new WmNativeWidgetAura(widget_, ShouldRemoveStandardFrame(*properties));
  window_ = native_widget->GetNativeView();
  window_->SetProperty(kNonClientFrameControllerKey, this);
  window_->SetProperty(kWidgetCreationTypeKey, WidgetCreationType::FOR_CLIENT);
  window_->AddObserver(this);
  params.native_widget = native_widget;
  aura::SetWindowType(window_, window_type);
  for (auto& property_pair : *properties) {
    property_converter->SetPropertyFromTransportValue(
        window_, property_pair.first, &property_pair.second);
  }
  // Applying properties will have set the show state if specified.
  // NativeWidgetAura resets the show state from |params|, so we need to update
  // |params|.
  params.show_state = window_->GetProperty(aura::client::kShowStateKey);
  widget_->Init(params);
  did_init_native_widget_ = true;

  // Only the caption draws any content. So the caption has its own layer (see
  // above in WmNativeWidgetAura::CreateNonClientFrameView()). The rest of the
  // region needs to take part in occlusion in the compositor, but not generate
  // any content to draw. So the layer is marked as opaque and to draw
  // solid-color (but the color is transparent, so nothing is actually drawn).
  ui::Layer* layer = widget_->GetNativeWindow()->layer();
  layer->SetColor(SK_ColorTRANSPARENT);
  layer->SetFillsBoundsOpaquely(true);
}

// static
NonClientFrameController* NonClientFrameController::Get(aura::Window* window) {
  return window->GetProperty(kNonClientFrameControllerKey);
}

// static
gfx::Insets NonClientFrameController::GetPreferredClientAreaInsets() {
  return gfx::Insets(
      GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption).height(), 0, 0, 0);
}

// static
int NonClientFrameController::GetMaxTitleBarButtonWidth() {
  return GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption).width() * 3;
}

void NonClientFrameController::StoreCursor(const ui::Cursor& cursor) {
  static_cast<WmNativeWidgetAura*>(widget_->native_widget())
      ->set_cursor(cursor);
}

base::string16 NonClientFrameController::GetWindowTitle() const {
  if (!window_)
    return base::string16();

  base::string16 title = window_->GetTitle();

  if (window_->GetProperty(kWindowIsJanky))
    title += base::ASCIIToUTF16(" !! Not responding !!");

  return title;
}

bool NonClientFrameController::CanResize() const {
  return window_ && (window_->GetProperty(aura::client::kResizeBehaviorKey) &
                     ws::mojom::kResizeBehaviorCanResize) != 0;
}

bool NonClientFrameController::CanMaximize() const {
  return window_ && (window_->GetProperty(aura::client::kResizeBehaviorKey) &
                     ws::mojom::kResizeBehaviorCanMaximize) != 0;
}

bool NonClientFrameController::CanMinimize() const {
  return window_ && (window_->GetProperty(aura::client::kResizeBehaviorKey) &
                     ws::mojom::kResizeBehaviorCanMinimize) != 0;
}

bool NonClientFrameController::CanActivate() const {
  // kCanFocus is used for both focus and activation.
  return window_ && window_->GetProperty(ws::kCanFocus) &&
         views::WidgetDelegate::CanActivate();
}

bool NonClientFrameController::ShouldShowWindowTitle() const {
  return window_ && window_->GetProperty(aura::client::kTitleShownKey);
}

void NonClientFrameController::DeleteDelegate() {
  delete this;
}

views::Widget* NonClientFrameController::GetWidget() {
  return widget_;
}

const views::Widget* NonClientFrameController::GetWidget() const {
  return widget_;
}

views::View* NonClientFrameController::GetContentsView() {
  return contents_view_;
}

views::ClientView* NonClientFrameController::CreateClientView(
    views::Widget* widget) {
  DCHECK(!contents_view_);
  contents_view_ = new ContentsViewMus();  // Owned by views hierarchy.
  return new ClientViewMus(widget, contents_view_, this);
}

void NonClientFrameController::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  // Properties are applied before the call to InitNativeWidget(). Ignore
  // processing changes in this case as the Widget is not in a state where we
  // can use it yet.
  if (!did_init_native_widget_)
    return;

  if (key == kWindowIsJanky || key == aura::client::kTitleKey ||
      key == aura::client::kTitleShownKey) {
    widget_->UpdateWindowTitle();
    widget_->non_client_view()->frame_view()->SchedulePaint();
  } else if (key == aura::client::kResizeBehaviorKey) {
    widget_->OnSizeConstraintsChanged();
  }
}

void NonClientFrameController::OnWindowDestroyed(aura::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

NonClientFrameController::~NonClientFrameController() {
  if (window_)
    window_->RemoveObserver(this);
}

}  // namespace ash
