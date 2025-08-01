// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/handoff_button_controller.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// A fixed vertical offset from the top of the window, used when the tab
// strip is not visible (e.g., in immersive fullscreen).
constexpr int kHandoffButtonTopOffset = 8;

std::unique_ptr<views::NonClientFrameView> CreateHandoffButtonFrameView(
    views::Widget* widget) {
  const gfx::Insets margins = gfx::Insets::VH(12, 20);
  auto frame_view =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), margins);
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::Arrow::NONE,
      views::BubbleBorder::Shadow::STANDARD_SHADOW);
  // TODO(crbug.com/422541242): Use ChromeLayoutProvider instead of hardcoding
  // this value.
  border->set_rounded_corners(gfx::RoundedCornersF(48));
  border->set_draw_border_stroke(false);
  frame_view->SetBubbleBorder(std::move(border));
  // TODO(crbug.com/422541242): Update color to match spec.
  frame_view->SetBackgroundColor(ui::kColorTextfieldBackground);
  return frame_view;
}

}  // namespace
namespace actor::ui {

using enum HandoffButtonState::ControlOwnership;
using ::ui::ImageModel;

HandoffButtonWidget::HandoffButtonWidget() = default;
HandoffButtonWidget::~HandoffButtonWidget() = default;

void HandoffButtonWidget::SetHoveredCallback(
    base::RepeatingCallback<void(bool)> callback) {
  hover_callback_ = std::move(callback);
}

void HandoffButtonWidget::OnMouseEvent(::ui::MouseEvent* event) {
  switch (event->type()) {
    case ::ui::EventType::kMouseEntered:
      hover_callback_.Run(true);
      break;
    case ::ui::EventType::kMouseExited:
      hover_callback_.Run(false);
      break;
    default:
      break;
  }
  views::Widget::OnMouseEvent(event);
}

HandoffButtonController::HandoffButtonController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {}

HandoffButtonController::~HandoffButtonController() = default;

void HandoffButtonController::UpdateState(const HandoffButtonState& state,
                                          bool is_visible) {
  is_active_ = state.is_active;
  if (!is_active_) {
    CloseButton(views::Widget::ClosedReason::kUnspecified);
    return;
  }
  is_visible_ = is_visible;
  ownership_ = state.controller;

  std::u16string text;
  ImageModel icon;
  // TODO(crbug.com/422541242): Update icon color to match spec.
  switch (state.controller) {
    case kActor:
      text = TAKE_OVER_TASK_TEXT;
      icon = ImageModel::FromVectorIcon(
          vector_icons::kSelectWindowChromeRefreshIcon, SK_ColorDKGRAY);
      break;
    case kClient:
      text = GIVE_TASK_BACK_TEXT;
      icon = ImageModel::FromVectorIcon(kScreensaverAutoIcon, SK_ColorDKGRAY);
      break;
  }

  // If the widget doesn't exist, create it with the correct initial state.
  if (!widget_) {
    CreateAndShowButton(text, icon);
  } else {
    // If it already exists, update its content.
    button_view_->SetText(text);
    button_view_->SetImageModel(views::Button::STATE_NORMAL, icon);
    UpdateBounds();
  }

  // TODO(crbug.com/422541242): Add Z-order logic.

  UpdateVisibility();
}

void HandoffButtonController::CreateAndShowButton(const std::u16string& text,
                                                  const ImageModel& icon) {
  CHECK(!widget_);

  auto* tab_dialog_manager = GetTabDialogManager();

  // Create the button view.
  auto button_view = std::make_unique<views::LabelButton>(
      base::BindRepeating(&HandoffButtonController::OnButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      text);
  button_view_ = button_view.get();
  // TODO(crbug.com/422541242): Update color to match spec.
  button_view_->SetEnabledTextColors(SK_ColorDKGRAY);
  button_view_->SetImageModel(views::Button::STATE_NORMAL, icon);

  auto widget_delegate = std::make_unique<views::WidgetDelegate>();
  widget_delegate->SetContentsView(std::move(button_view));
  widget_delegate->SetModalType(::ui::mojom::ModalType::kNone);
  widget_delegate->SetAccessibleWindowRole(ax::mojom::Role::kAlert);
  widget_delegate->SetShowCloseButton(false);
  widget_delegate->SetNonClientFrameViewFactory(
      base::BindRepeating(&CreateHandoffButtonFrameView));
  delegate_ = std::move(widget_delegate);

  // Create the Widget using the delegate.
  auto widget = std::make_unique<HandoffButtonWidget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET);
  params.delegate = delegate_.get();
  params.parent = tab_dialog_manager->GetHostWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.remove_standard_frame = true;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.autosize = false;
  params.name = "HandoffButtonWidget";
  widget->Init(std::move(params));

  auto tab_dialog_params = std::make_unique<tabs::TabDialogManager::Params>();
  tab_dialog_params->close_on_navigate = false;
  tab_dialog_params->close_on_detach = false;
  tab_dialog_params->disable_input = false;
  tab_dialog_params->animated = false;
  tab_dialog_params->should_show_callback = base::BindRepeating(
      &HandoffButtonController::ShouldShowButton, base::Unretained(this));
  tab_dialog_params->get_dialog_bounds =
      base::BindRepeating(&HandoffButtonController::GetHandoffButtonBounds,
                          base::Unretained(this), widget.get());

  tab_dialog_manager->ShowDialog(widget.get(), std::move(tab_dialog_params));
  widget_ = std::move(widget);
  widget_->SetHoveredCallback(
      base::BindRepeating(&HandoffButtonController::UpdateButtonHoverStatus,
                          weak_ptr_factory_.GetWeakPtr()));

  widget_->MakeCloseSynchronous(base::BindOnce(
      &HandoffButtonController::CloseButton, weak_ptr_factory_.GetWeakPtr()));
}

void HandoffButtonController::ShouldShowButton(bool& show) {
  show = is_active_ && is_visible_;
}

gfx::Rect HandoffButtonController::GetHandoffButtonBounds(
    views::Widget* widget) {
  const gfx::Size preferred_size =
      widget->GetContentsView()->GetPreferredSize();

  auto* anchor_view = tab_interface_->GetBrowserWindowInterface()->GetWebView();
  if (!anchor_view) {
    return gfx::Rect(preferred_size);
  }
  const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();

  const int x =
      anchor_bounds.x() + (anchor_bounds.width() - preferred_size.width()) / 2;

  // Calculate the Y coordinate based on tab strip visibility.
  const bool is_tab_strip_visible =
      tab_interface_->GetBrowserWindowInterface()->IsTabStripVisible();

  const int y =
      is_tab_strip_visible
          // Vertically center the button on the top edge of the anchor.
          ? anchor_bounds.y() - preferred_size.height() / 2
          // Position with a fixed offset from the top of the anchor.
          : anchor_bounds.y() - kHandoffButtonTopOffset;

  return gfx::Rect({x, y}, preferred_size);
}

void HandoffButtonController::CloseButton(views::Widget::ClosedReason reason) {
  button_view_ = nullptr;
  if (widget_) {
    widget_->CloseNow();
    widget_.reset();
    delegate_.reset();
  }
}

void HandoffButtonController::UpdateButtonHoverStatus(bool is_hovered) {
  GetTabController()->SetHandoffButtonHoverStatus(is_hovered);
}

void HandoffButtonController::OnButtonPressed() {
  // If the Actor is currently in control, pressing the button
  // flips the state and pauses the task.
  if (ownership_ == kActor) {
    GetTabController()->SetActorTaskPaused();
  } else {
    GetTabController()->SetActorTaskResume();
  }
}

void HandoffButtonController::UpdateBounds() {
  GetTabDialogManager()->UpdateModalDialogBounds();
}

void HandoffButtonController::UpdateVisibility() {
  GetTabDialogManager()->UpdateDialogVisibility();
}

tabs::TabDialogManager* HandoffButtonController::GetTabDialogManager() {
  auto* features = tab_interface_->GetTabFeatures();
  CHECK(features);
  return features->tab_dialog_manager();
}

ActorUiTabControllerInterface* HandoffButtonController::GetTabController() {
  return tab_interface_->GetTabFeatures()->actor_ui_tab_controller();
}

}  // namespace actor::ui
