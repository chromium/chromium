// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view.h"

#include <algorithm>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_container_view_animator.h"
#include "ash/assistant/ui/assistant_main_view.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_web_view.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SK_ColorWHITE;

// Window properties.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kOnlyAllowMouseClickEvents, false);

// AssistantContainerEventTargeter ---------------------------------------------

class AssistantContainerEventTargeter : public aura::WindowTargeter {
 public:
  AssistantContainerEventTargeter() = default;
  ~AssistantContainerEventTargeter() override = default;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    if (window->GetProperty(kOnlyAllowMouseClickEvents)) {
      if (event.type() != ui::ET_MOUSE_PRESSED &&
          event.type() != ui::ET_MOUSE_RELEASED) {
        return false;
      }
    }
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantContainerEventTargeter);
};

// AssistantContainerLayout ----------------------------------------------------

// The AssistantContainerLayout calculates preferred size to fit the largest
// visible child. Children that are not visible are not factored in. During
// layout, children are horizontally centered and bottom aligned.
class AssistantContainerLayout : public views::LayoutManager {
 public:
  AssistantContainerLayout() = default;
  ~AssistantContainerLayout() override = default;

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    int preferred_width = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      const views::View* child = host->child_at(i);

      // We do not include invisible children in our size calculation.
      if (!child->visible())
        continue;

      // Our preferred width is the width of our largest visible child.
      preferred_width =
          std::max(child->GetPreferredSize().width(), preferred_width);
    }

    return gfx::Size(preferred_width,
                     GetPreferredHeightForWidth(host, preferred_width));
  }

  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override {
    int preferred_height = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      const views::View* child = host->child_at(i);

      // We do not include invisible children in our size calculation.
      if (!child->visible())
        continue;

      // Our preferred height is the height of our largest visible child.
      preferred_height =
          std::max(child->GetHeightForWidth(width), preferred_height);
    }

    return preferred_height;
  }

  void Layout(views::View* host) override {
    const int host_center_x = host->GetBoundsInScreen().CenterPoint().x();
    const int host_height = host->height();

    for (int i = 0; i < host->child_count(); ++i) {
      views::View* child = host->child_at(i);

      const gfx::Size child_size = child->GetPreferredSize();

      // Children are horizontally centered. This means that both the |host|
      // and child views share the same center x-coordinate relative to the
      // screen. We use this center value when placing our children because
      // deriving center from the host width causes rounding inconsistencies
      // that are especially noticeable during animation.
      gfx::Point child_center(host_center_x, /*y=*/0);
      views::View::ConvertPointFromScreen(host, &child_center);
      int child_left = child_center.x() - child_size.width() / 2;

      // Children are bottom aligned.
      int child_top = host_height - child_size.height();

      child->SetBounds(child_left, child_top, child_size.width(),
                       child_size.height());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantContainerLayout);
};

}  // namespace

// AssistantContainerView ------------------------------------------------------

AssistantContainerView::AssistantContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      animator_(
          AssistantContainerViewAnimator::Create(assistant_controller_, this)),
      focus_traversable_(this) {
  UpdateAnchor();

  set_accept_events(true);
  set_close_on_deactivate(false);
  set_color(kBackgroundColor);
  set_margins(gfx::Insets());
  set_shadow(views::BubbleBorder::Shadow::NO_ASSETS);
  set_title_margins(gfx::Insets());

  views::BubbleDialogDelegateView::CreateBubble(this);

  // Corner radius can only be set after bubble creation.
  GetBubbleFrameView()->bubble_border()->SetCornerRadius(
      assistant_controller_->ui_controller()->model()->ui_mode() ==
              AssistantUiMode::kMiniUi
          ? kMiniUiCornerRadiusDip
          : kCornerRadiusDip);

  // Initialize non-client view layer.
  GetBubbleFrameView()->SetPaintToLayer();
  GetBubbleFrameView()->layer()->SetFillsBoundsOpaquely(false);

  // The AssistantController owns the view hierarchy to which
  // AssistantContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->ui_controller()->AddModelObserver(this);

  // Initialize |animator_| only after AssistantContainerView has been
  // fully constructed to give it a chance to perform additional initialization.
  animator_->Init();
}

AssistantContainerView::~AssistantContainerView() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

// static
void AssistantContainerView::OnlyAllowMouseClickEvents(aura::Window* window) {
  window->SetProperty(kOnlyAllowMouseClickEvents, true);
}

const char* AssistantContainerView::GetClassName() const {
  return "AssistantContainerView";
}

void AssistantContainerView::AddedToWidget() {
  GetWidget()->GetNativeWindow()->SetEventTargeter(
      std::make_unique<AssistantContainerEventTargeter>());
}

ax::mojom::Role AssistantContainerView::GetAccessibleWindowRole() const {
  return ax::mojom::Role::kWindow;
}

int AssistantContainerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

views::FocusTraversable* AssistantContainerView::GetFocusTraversable() {
  return &focus_traversable_;
}

void AssistantContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantContainerView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // Do nothing. We override this method to prevent a super class implementation
  // from taking effect which would otherwise cause ChromeVox to read the entire
  // Assistant view hierarchy.
}

void AssistantContainerView::SizeToContents() {
  // We override this method to increase its visibility.
  views::BubbleDialogDelegateView::SizeToContents();
}

void AssistantContainerView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->context = Shell::Get()->GetRootWindowForNewWindows();
  params->corner_radius = kCornerRadiusDip;
  params->keep_on_top = true;
}

void AssistantContainerView::Init() {
  SetLayoutManager(std::make_unique<AssistantContainerLayout>());

  // We paint to our own layer. Some implementations of |animator_| mask to
  // bounds to clip child layers.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Main view.
  assistant_main_view_ = new AssistantMainView(assistant_controller_);
  AddChildView(assistant_main_view_);

  // Mini view.
  assistant_mini_view_ = new AssistantMiniView(assistant_controller_);
  assistant_mini_view_->set_delegate(assistant_controller_->ui_controller());
  AddChildView(assistant_mini_view_);

  // Web view.
  assistant_web_view_ = new AssistantWebView(assistant_controller_);
  AddChildView(assistant_web_view_);

  // Update the view state based on the current UI mode.
  OnUiModeChanged(assistant_controller_->ui_controller()->model()->ui_mode());
}

void AssistantContainerView::RequestFocus() {
  if (!GetWidget() || !GetWidget()->IsActive())
    return;

  switch (assistant_controller_->ui_controller()->model()->ui_mode()) {
    case AssistantUiMode::kMiniUi:
      if (assistant_mini_view_)
        assistant_mini_view_->RequestFocus();
      break;
    case AssistantUiMode::kMainUi:
      if (assistant_main_view_)
        assistant_main_view_->RequestFocus();
      break;
    case AssistantUiMode::kWebUi:
      if (assistant_web_view_)
        assistant_web_view_->RequestFocus();
      break;
  }
}

void AssistantContainerView::UpdateAnchor() {
  // Align to the bottom, horizontal center of the current usable work area.
  const gfx::Rect& usable_work_area =
      assistant_controller_->ui_controller()->model()->usable_work_area();
  const gfx::Rect anchor =
      gfx::Rect(usable_work_area.x(), usable_work_area.bottom(),
                usable_work_area.width(), 0);
  SetAnchorRect(anchor);
  SetArrow(views::BubbleBorder::Arrow::BOTTOM_CENTER);
}

void AssistantContainerView::OnUiModeChanged(AssistantUiMode ui_mode) {
  for (int i = 0; i < child_count(); ++i) {
    child_at(i)->SetVisible(false);
  }

  switch (ui_mode) {
    case AssistantUiMode::kMiniUi:
      assistant_mini_view_->SetVisible(true);
      break;
    case AssistantUiMode::kMainUi:
      assistant_main_view_->SetVisible(true);
      break;
    case AssistantUiMode::kWebUi:
      assistant_web_view_->SetVisible(true);
      break;
  }

  PreferredSizeChanged();
  RequestFocus();
}

void AssistantContainerView::OnUsableWorkAreaChanged(
    const gfx::Rect& usable_work_area) {
  UpdateAnchor();

  // Call PreferredSizeChanged() to update animation params to avoid undesired
  // effects (e.g. resize animation of Assistant UI when zooming in/out screen).
  PreferredSizeChanged();
}

views::View* AssistantContainerView::FindFirstFocusableView() {
  if (!GetWidget() || !GetWidget()->IsActive())
    return nullptr;

  switch (assistant_controller_->ui_controller()->model()->ui_mode()) {
    case AssistantUiMode::kMainUi:
      // AssistantMainView will sometimes explicitly specify a view to be
      // focused first. Other times it may defer to views::FocusSearch.
      return assistant_main_view_
                 ? assistant_main_view_->FindFirstFocusableView()
                 : nullptr;
    case AssistantUiMode::kMiniUi:
    case AssistantUiMode::kWebUi:
      // Default views::FocusSearch behavior is acceptable.
      return nullptr;
  }
}

SkColor AssistantContainerView::GetBackgroundColor() const {
  return kBackgroundColor;
}

int AssistantContainerView::GetCornerRadius() const {
  return GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius();
}

void AssistantContainerView::SetCornerRadius(int corner_radius) {
  GetBubbleFrameView()->bubble_border()->SetCornerRadius(corner_radius);
}

ui::Layer* AssistantContainerView::GetNonClientViewLayer() {
  return GetBubbleFrameView()->layer();
}

}  // namespace ash
