// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/exit_warning_handler.h"

#include <memory>

#include "ash/accelerators/accelerator_lookup.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

using AcceleratorDetails = AcceleratorLookup::AcceleratorDetails;

const int64_t kTimeOutMilliseconds = 2000;
// Color of the text of the warning message.
const SkColor kTextColor = SK_ColorWHITE;
// Color of the window background.
const SkColor kWindowBackgroundColor = SkColorSetARGB(0xC0, 0x0, 0x0, 0x0);
// Radius of the rounded corners of the window.
const int kWindowCornerRadius = 2;
const int kHorizontalMarginAroundText = 100;
const int kVerticalMarginAroundText = 100;

class ExitWarningWidgetDelegateView : public views::WidgetDelegateView {
 public:
  ExitWarningWidgetDelegateView() : text_width_(0) {
    std::vector<AcceleratorDetails> accelerators =
        Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
            AcceleratorAction::kExit);
    CHECK(!accelerators.empty());
    // TODO(jimmyxgong): For now fetch the first accelerator of the list. But
    // maybe there's a possibility to check which accelerator was most recently
    // pressed.
    text_ = l10n_util::GetStringFUTF16(
        IDS_ASH_SIGN_OUT_WARNING_POPUP_TEXT_DYNAMIC,
        AcceleratorLookup::GetAcceleratorDetailsText(accelerators[0]));

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& font_list =
        rb.GetFontList(ui::ResourceBundle::LargeFont);
    text_width_ = gfx::GetStringWidth(text_, font_list);
    SetPreferredSize(
        gfx::Size(text_width_ + kHorizontalMarginAroundText,
                  font_list.GetHeight() + kVerticalMarginAroundText));
    auto label = std::make_unique<views::Label>();
    label->SetText(text_);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label->SetFontList(font_list);
    label->SetEnabledColor(kTextColor);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    AddChildView(std::move(label));
    SetLayoutManager(std::make_unique<views::FillLayout>());

    GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_SIGN_OUT_WARNING_POPUP_TEXT_ACCESSIBLE));
  }

  ExitWarningWidgetDelegateView(const ExitWarningWidgetDelegateView&) = delete;
  ExitWarningWidgetDelegateView& operator=(
      const ExitWarningWidgetDelegateView&) = delete;

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(kWindowBackgroundColor);
    canvas->DrawRoundRect(GetLocalBounds(), kWindowCornerRadius, flags);
    views::WidgetDelegateView::OnPaint(canvas);
  }

 private:
  std::u16string text_;
  int text_width_;
};

}  // namespace

ExitWarningHandler::ExitWarningHandler()
    : state_(IDLE), stub_timer_for_test_(false) {}

ExitWarningHandler::~ExitWarningHandler() {
  // Note: If a timer is outstanding, it is stopped in its destructor.
  Hide();
}

void ExitWarningHandler::HandleAccelerator() {
  switch (state_) {
    case IDLE:
      state_ = WAIT_FOR_DOUBLE_PRESS;
      Show();
      StartTimer();
      base::RecordAction(base::UserMetricsAction("Accel_Exit_First_Q"));
      break;
    case WAIT_FOR_DOUBLE_PRESS:
      state_ = EXITING;
      CancelTimer();
      Hide();
      base::RecordAction(base::UserMetricsAction("Accel_Exit_Second_Q"));
      Shell::Get()->session_controller()->RequestSignOut();
      break;
    case EXITING:
      break;
  }
}

void ExitWarningHandler::TimerAction() {
  Hide();
  if (state_ == WAIT_FOR_DOUBLE_PRESS)
    state_ = IDLE;
}

void ExitWarningHandler::StartTimer() {
  if (stub_timer_for_test_)
    return;
  timer_.Start(FROM_HERE, base::Milliseconds(kTimeOutMilliseconds), this,
               &ExitWarningHandler::TimerAction);
}

void ExitWarningHandler::CancelTimer() {
  timer_.Stop();
}

void ExitWarningHandler::Show() {
  if (widget_)
    return;
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  ExitWarningWidgetDelegateView* delegate = new ExitWarningWidgetDelegateView;
  gfx::Size rs = root_window->bounds().size();
  gfx::Size ps = delegate->GetPreferredSize();
  gfx::Rect bounds((rs.width() - ps.width()) / 2,
                   (rs.height() - ps.height()) / 3, ps.width(), ps.height());
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);

  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.delegate = delegate;
  params.bounds = bounds;
  params.name = "ExitWarningWindow";
  params.parent =
      root_window->GetChildById(kShellWindowId_SettingBubbleContainer);
  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_->Show();

  delegate->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void ExitWarningHandler::Hide() {
  widget_.reset();
}

}  // namespace ash
