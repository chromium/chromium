// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notification_helper.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace policy {

namespace {

// The name of the bubble.
constexpr char kBubbleName[] = "ClipboardDlpBubble";

// The corner radius of the bubble.
constexpr int kBubbleCornerRadius = 8;
constexpr gfx::RoundedCornersF kCornerRadii(kBubbleCornerRadius);

// The blur radius for the bubble background.
constexpr int kBubbleBlurRadius = 80;

// The alpha component of the bubble background.
constexpr float kBubbleBackgroundAlpha = 0.8f;

// The size of the managed icon.
constexpr int kManagedIconSize = 20;

// The maximum width of the label.
constexpr int kBubbleWidth = 360;

// The spacing between the icon and label in the bubble.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the bubble border with its inner contents.
constexpr int kBubblePadding = 16;

// The line height of the bubble text.
constexpr int kLineHeight = 20;

// The insets of the bubble borders.
constexpr gfx::Insets kBubbleBorderInsets(1);

// Clipboard ARC toast ID.
constexpr char kClipboardArcToastId[] = "clipboard_dlp_block_arc";

// Clipboard Crostini toast ID.
constexpr char kClipboardCrostiniToastId[] = "clipboard_dlp_block_crostini";

// Clipboard Plugin VM toast ID.
constexpr char kClipboardPluginVmToastId[] = "clipboard_dlp_block_plugin_vm";

// The duration of the clipboard toast.
constexpr int kToastDurationMs = 2500;

// The font of the text used in the bubble.
constexpr char kTextFont[] = "Roboto, 13px";

// The height of the dismiss button.
constexpr int kButtonHeight = 32;

// The padding which separates the button border with its inner contents.
constexpr int kButtonPadding = 16;

// The spacing between the button border and label.
constexpr int kButtonLabelSpacing = 8;

constexpr base::TimeDelta kBubbleBoundsAnimationTime =
    base::TimeDelta::FromMilliseconds(250);

class DismissButton : public views::LabelButton {
 public:
  DismissButton() {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

    const base::string16 button_label(l10n_util::GetStringUTF16(
        IDS_POLICY_DLP_CLIPBOARD_BLOCK_DISMISS_BUTTON));
    SetText(button_label);
    label()->SetFontList(gfx::FontList(kTextFont));
    SetTextColor(ButtonState::STATE_NORMAL, gfx::kGoogleBlue800);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetSize({gfx::GetStringWidth(button_label, gfx::FontList(kTextFont)) +
                 2 * kButtonPadding,
             kButtonHeight});
  }

  int GetLabelWidth() { return label()->bounds().width(); }

  DismissButton(const DismissButton&) = delete;
  DismissButton& operator=(const DismissButton&) = delete;

  ~DismissButton() override = default;
};

// This inline bubble shown for disabled copy/paste.
class ClipboardBubbleView : public views::View {
 public:
  explicit ClipboardBubbleView(const base::string16& text) {
    // TODO(crbug.com/1150740): Change colors in case of dark mode.

    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetColor(
        SkColorSetA(SK_ColorWHITE, SK_AlphaOPAQUE * kBubbleBackgroundAlpha));
    if (ash::features::IsBackgroundBlurEnabled())
      layer()->SetBackgroundBlur(kBubbleBlurRadius);
    layer()->SetRoundedCornerRadius(kCornerRadii);

    // Add the managed icon.
    SkColor icon_color = SK_ColorGRAY;
    clipboard_icon_ = AddChildView(std::make_unique<views::ImageView>());
    clipboard_icon_->SetPaintToLayer();
    clipboard_icon_->layer()->SetFillsBoundsOpaquely(false);
    clipboard_icon_->SetBounds(kBubblePadding, kBubblePadding, kManagedIconSize,
                               kManagedIconSize);
    clipboard_icon_->SetImage(gfx::CreateVectorIcon(
        vector_icons::kBusinessIcon, kManagedIconSize, icon_color));

    // Add the bubble text.
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetPosition(gfx::Point(
        kBubblePadding + kManagedIconSize + kIconLabelSpacing, kBubblePadding));

    // Set the styling of the text.
    // TODO(crbug.com/1150741): Handle RTL.
    label_->SetText(text);
    label_->SetFontList(gfx::FontList("Roboto, 13px"));
    label_->SetEnabledColor(SK_ColorBLACK);
    label_->SetLineHeight(kLineHeight);
    label_->SetMultiLine(true);
    label_->SizeToFit(kBubbleWidth - 2 * kBubblePadding - kManagedIconSize -
                      kIconLabelSpacing);
    label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    // Bubble borders
    border_ = AddChildView(std::make_unique<views::ImageView>());
    border_->SetPaintToLayer();
    border_->layer()->SetFillsBoundsOpaquely(false);
    auto shadow_border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::STANDARD_SHADOW,
        SK_ColorTRANSPARENT);
    shadow_border->SetCornerRadius(kBubbleCornerRadius);
    shadow_border->set_background_color(SK_ColorTRANSPARENT);
    shadow_border->set_insets(kBubbleBorderInsets);
    border_->SetSize({kBubbleWidth, INT_MAX});
    border_->SetBorder(std::move(shadow_border));

    // Add "Got it" button.
    button_ = AddChildView(std::make_unique<DismissButton>());
    button_->SetPaintToLayer();
    button_->layer()->SetFillsBoundsOpaquely(false);
    button_->SetPosition(
        gfx::Point(kBubbleWidth - kBubblePadding - button_->width(),
                   kBubblePadding + label_->height() + kButtonLabelSpacing));
  }

  ~ClipboardBubbleView() override = default;

  views::Label* label_ = nullptr;
  views::ImageView* clipboard_icon_ = nullptr;
  views::ImageView* border_ = nullptr;
  DismissButton* button_ = nullptr;
};

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
                                 ClipboardBubbleView* bubble_view) {
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

  // Calculate the bubble size to ensure the label text accurately fits.
  const int bubble_height =
      2 * kBubblePadding + bubble_view->label_->bounds().height() +
      kButtonLabelSpacing + bubble_view->button_->height();

  bubble_view->border_->SetSize({kBubbleWidth, bubble_height});

  const gfx::Rect widget_bounds = gfx::Rect(caret_bounds.x(), caret_bounds.y(),
                                            kBubbleWidth, bubble_height);

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

}  // namespace

void DlpClipboardNotificationHelper::NotifyBlockedPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  DCHECK(data_src);
  DCHECK(data_src->origin());
  const base::string16 host_name =
      base::UTF8ToUTF16(data_src->origin()->host());

  if (data_dst) {
    if (data_dst->type() == ui::EndpointType::kCrostini) {
      ShowClipboardBlockToast(
          kClipboardCrostiniToastId,
          l10n_util::GetStringFUTF16(
              IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
              l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kPluginVm) {
      ShowClipboardBlockToast(
          kClipboardPluginVmToastId,
          l10n_util::GetStringFUTF16(
              IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));
      return;
    }
    if (data_dst->type() == ui::EndpointType::kArc) {
      ShowClipboardBlockToast(
          kClipboardArcToastId,
          l10n_util::GetStringFUTF16(
              IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
              l10n_util::GetStringUTF16(IDS_POLICY_DLP_ANDROID_APPS)));
      return;
    }
  }
  ShowClipboardBlockBubble(l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE, host_name));
}

void DlpClipboardNotificationHelper::ShowClipboardBlockBubble(
    const base::string16& text) {
  widget_ = std::make_unique<views::Widget>();

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = kBubbleName;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = nullptr;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;

  widget_->Init(std::move(params));
  auto* bubble_view =
      widget_->SetContentsView(std::make_unique<ClipboardBubbleView>(text));

  bubble_view->button_->SetCallback(
      base::BindRepeating(&DlpClipboardNotificationHelper::OnWidgetClosing,
                          base::Unretained(this), widget_.get()));

  CalculateAndSetWidgetBounds(widget_.get(), bubble_view);

  widget_->Show();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DlpClipboardNotificationHelper::OnWidgetClosing,
                     base::Unretained(this),
                     widget_.get()),  // Safe as DlpClipboardNotificationHelper
                                      // owns `widget_` and outlives it.
      base::TimeDelta::FromMilliseconds(kToastDurationMs));
}

void DlpClipboardNotificationHelper::ShowClipboardBlockToast(
    const std::string& id,
    const base::string16& text) {
  ash::ToastData toast(id, text, kToastDurationMs,
                       /*dismiss_text=*/base::nullopt);
  toast.is_managed = true;
  ash::ToastManager::Get()->Show(toast);
}

void DlpClipboardNotificationHelper::OnWidgetClosing(views::Widget* widget) {
  if (widget == widget_.get())
    widget_.reset();
}

void DlpClipboardNotificationHelper::OnWidgetDestroyed(views::Widget* widget) {
  if (widget == widget_.get())
    widget_.reset();
}

}  // namespace policy
