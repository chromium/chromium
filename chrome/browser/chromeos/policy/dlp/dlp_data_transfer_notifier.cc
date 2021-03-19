// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_data_transfer_notifier.h"

#include "ash/public/cpp/window_tree_host_lookup.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace policy {

namespace {

// The name of the bubble.
constexpr char kBubbleName[] = "ClipboardDlpBubble";

constexpr base::TimeDelta kBubbleBoundsAnimationTime =
    base::TimeDelta::FromMilliseconds(250);

bool IsRectContainedByAnyDisplay(const gfx::Rect& rect) {
  const std::vector<display::Display>& displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const auto& display : displays) {
    if (display.bounds().Contains(rect))
      return true;
  }
  return false;
}

void CalculateAndSetWidgetBounds(views::Widget* widget,
                                 const gfx::Size& bubble_size) {
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display = screen->GetPrimaryDisplay();
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());

  ui::TextInputClient* text_input_client =
      host->GetInputMethod()->GetTextInputClient();

  // `text_input_client` may be null. For example, in clamshell mode and without
  // any window open.
  if (!text_input_client)
    return;

  gfx::Rect caret_bounds = text_input_client->GetCaretBounds();

  // Note that the width of caret's bounds may be zero in some views (such as
  // the search bar of Google search web page). So we cannot use
  // gfx::Size::IsEmpty() here. In addition, the applications using IFrame may
  // provide unreliable `caret_bounds` which are not fully contained by the
  // display bounds.
  const bool caret_bounds_are_valid = caret_bounds.size() != gfx::Size() &&
                                      IsRectContainedByAnyDisplay(caret_bounds);
  if (!caret_bounds_are_valid) {
    caret_bounds.set_origin(
        display::Screen::GetScreen()->GetCursorScreenPoint());
  }

  const gfx::Rect widget_bounds =
      gfx::Rect(caret_bounds.x(), caret_bounds.y(), bubble_size.width(),
                bubble_size.height());

  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (widget->GetWindowBoundsInScreen().size() != gfx::Size()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        widget->GetLayer()->GetAnimator());
    settings->SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings->SetTransitionDuration(kBubbleBoundsAnimationTime);
    settings->SetTweenType(gfx::Tween::EASE_OUT);
  }

  widget->SetBounds(widget_bounds);
}

views::Widget::InitParams GetWidgetInitParams() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.z_order = ui::ZOrderLevel::kNormal;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = kBubbleName;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = nullptr;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  return params;
}

}  // namespace

DlpDataTransferNotifier::DlpDataTransferNotifier() = default;
DlpDataTransferNotifier::~DlpDataTransferNotifier() {
  if (widget_) {
    widget_->RemoveObserver(this);
    CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);
  }
}

void DlpDataTransferNotifier::ShowBlockBubble(const std::u16string& text) {
  InitWidget();
  ClipboardBlockBubble* bubble =
      widget_->SetContentsView(std::make_unique<ClipboardBlockBubble>(text));
  bubble->SetDismissCallback(base::BindRepeating(
      &DlpDataTransferNotifier::CloseWidget, base::Unretained(this),
      widget_.get(), views::Widget::ClosedReason::kCancelButtonClicked));
  ResizeAndShowWidget(bubble->GetBubbleSize(), kClipboardDlpBlockDurationMs);
}

void DlpDataTransferNotifier::ShowWarningBubble(
    const std::u16string& text,
    base::RepeatingCallback<void(views::Widget*)> proceed_cb,
    base::RepeatingCallback<void(views::Widget*)> cancel_cb) {
  InitWidget();
  ClipboardWarnBubble* bubble =
      widget_->SetContentsView(std::make_unique<ClipboardWarnBubble>(text));
  bubble->SetProceedCallback(
      base::BindRepeating(std::move(proceed_cb), widget_.get()));
  bubble->SetDismissCallback(
      base::BindRepeating(std::move(cancel_cb), widget_.get()));
  ResizeAndShowWidget(bubble->GetBubbleSize(), kClipboardDlpWarnDurationMs);
}

void DlpDataTransferNotifier::CloseWidget(views::Widget* widget,
                                          views::Widget::ClosedReason reason) {
  if (widget_) {
    DCHECK_EQ(widget, widget_.get());
    widget_closing_timer_.Stop();
    widget_->CloseWithReason(reason);
  }
}

void DlpDataTransferNotifier::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, widget_.get());
  widget_->RemoveObserver(this);
  widget_.reset();
  widget_closing_timer_.Stop();
}

void DlpDataTransferNotifier::OnWidgetActivationChanged(views::Widget* widget,
                                                        bool active) {
  if (!active && widget->IsVisible())
    CloseWidget(widget, views::Widget::ClosedReason::kLostFocus);
}

void DlpDataTransferNotifier::InitWidget() {
  widget_ = std::make_unique<views::Widget>();
  widget_->Init(GetWidgetInitParams());
  widget_->AddObserver(this);
}

void DlpDataTransferNotifier::ResizeAndShowWidget(const gfx::Size& bubble_size,
                                                  int timeout_duration_ms) {
  DCHECK(widget_);

  CalculateAndSetWidgetBounds(widget_.get(), bubble_size);

  widget_->Show();

  widget_closing_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_duration_ms),
      base::BindOnce(&DlpDataTransferNotifier::CloseWidget,
                     base::Unretained(this),
                     widget_.get(),  // Safe as DlpClipboardNotificationHelper
                                     // owns `widget_` and outlives it.
                     views::Widget::ClosedReason::kUnspecified));
}

}  // namespace policy
