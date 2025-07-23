// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/handoff_button_controller.h"

#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

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

HandoffButtonController::HandoffButtonController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {}

HandoffButtonController::~HandoffButtonController() = default;

void HandoffButtonController::UpdateState(const HandoffButtonState& state,
                                          bool is_visible) {
  is_active_ = state.is_active;
  is_visible_ = is_visible;
  if (!is_active_) {
    CloseButton(views::Widget::ClosedReason::kUnspecified);
    return;
  }

  // TODO(crbug.com/422541242): Update placeholder text and icon.
  if (!widget_) {
    CreateAndShowButton(
        u"Take over task",
        ::ui::ImageModel::FromVectorIcon(
            vector_icons::kSelectWindowChromeRefreshIcon, SK_ColorDKGRAY));
  }

  // TODO(crbug.com/422541242): Add Z-order logic.

  // TODO(crbug.com/422541242): Update dialog visibility via the
  // TabDialogManager.
}

void HandoffButtonController::CreateAndShowButton(
    const std::u16string& text,
    const ::ui::ImageModel& icon) {
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
  auto widget = std::make_unique<views::Widget>();
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
  tab_dialog_params->animated = true;
  tab_dialog_params->should_show_callback = base::BindRepeating(
      &HandoffButtonController::ShouldShowButton, base::Unretained(this));

  tab_dialog_manager->ShowDialog(widget.get(), std::move(tab_dialog_params));
  widget_ = std::move(widget);

  widget_->MakeCloseSynchronous(base::BindOnce(
      &HandoffButtonController::CloseButton, weak_ptr_factory_.GetWeakPtr()));
}

void HandoffButtonController::ShouldShowButton(bool& show) {
  show = is_active_ && is_visible_;
}

void HandoffButtonController::CloseButton(views::Widget::ClosedReason reason) {
  button_view_ = nullptr;
  if (widget_) {
    widget_->CloseNow();
    widget_.reset();
    delegate_.reset();
  }
}

void HandoffButtonController::OnButtonPressed() {
  // TODO(crbug.com/422541242): Implement action callback logic.
}

tabs::TabDialogManager* HandoffButtonController::GetTabDialogManager() {
  auto* features = tab_interface_->GetTabFeatures();
  CHECK(features);
  return features->tab_dialog_manager();
}

}  // namespace actor::ui
