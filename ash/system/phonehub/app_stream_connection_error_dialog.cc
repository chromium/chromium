// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_connection_error_dialog.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/text_constants.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Offset to place dialog in the vertical center of bubble due to
// PhoneStatusView.
constexpr int kDialogVerticalOffset = 25;

constexpr int kDialogWidth = 330;

constexpr gfx::Insets kDialogContentInsets = gfx::Insets::VH(20, 24);
constexpr float kDialogRoundedCornerRadius = 16.0f;
constexpr int kDialogShadowElevation = 3;

constexpr int kIconSize = 25;

constexpr int kMarginBetweenIconAndTitle = 15;
constexpr int kMarginBetweenTitleAndBody = 15;
constexpr int kMarginBetweenBodyAndButtons = 20;
constexpr int kMarginBetweenButtons = 8;

// The real error dialog with content.
class ConnectionErrorDialogDelegateView : public views::WidgetDelegateView {
  METADATA_HEADER(ConnectionErrorDialogDelegateView, views::WidgetDelegateView)
 public:
  ConnectionErrorDialogDelegateView(
      StartTetheringCallback start_tethering_callback,
      bool is_on_different_network,
      bool is_phone_on_cellular)
      : start_tethering_callback_(std::move(start_tethering_callback)) {
    SetModalType(ui::mojom::ModalType::kWindow);

    SetPaintToLayer();
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(kDialogRoundedCornerRadius));

    SetBackground(views::CreateThemedRoundedRectBackground(
        static_cast<ui::ColorId>(cros_tokens::kCrosSysBaseElevated),
        kDialogRoundedCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kDialogRoundedCornerRadius,
        views::HighlightBorder::Type::kHighlightBorder1));

    view_shadow_ =
        std::make_unique<views::ViewShadow>(this, kDialogShadowElevation);
    view_shadow_->SetRoundedCornerRadius(kDialogRoundedCornerRadius);

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, kDialogContentInsets));

    // Add info icon.
    auto* icon_row = AddChildView(std::make_unique<views::View>());
    icon_row
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets()))
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
    icon_ = icon_row->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            kPhoneHubEcheErrorStatusIcon,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorWarning),
            kIconSize)));

    // Add dialog title.
    title_ =
        AddChildView(std::make_unique<views::Label>(l10n_util::GetStringUTF16(
            IDS_ASH_ECHE_APP_STREMING_ERROR_DIALOG_TITLE)));
    title_->SetProperty(views::kMarginsKey,
                        gfx::Insets::TLBR(kMarginBetweenIconAndTitle, 0, 0, 0));
    title_->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
    title_->SetTextStyle(views::style::STYLE_EMPHASIZED);
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetAutoColorReadabilityEnabled(false);

    TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosTitle1,
                                          *title_);
    title_->SetPaintToLayer();
    title_->layer()->SetFillsBoundsOpaquely(false);

    // Add dialog body.

    body_ = AddChildView(std::make_unique<views::StyledLabel>());

    std::u16string body_text;
    const std::u16string learn_more_link =
        l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE);
    // To record where "Learn more" text begin in the dialog body.
    size_t offset;
    if (is_phone_on_cellular) {
      body_text = l10n_util::GetStringFUTF16(
          IDS_ASH_ECHE_APP_STREAMING_ERROR_DIALOG_PHONE_ON_CELLULAR_TEXT,
          learn_more_link, &offset);
    } else if (is_on_different_network) {
      body_text = l10n_util::GetStringFUTF16(
          IDS_ASH_ECHE_APP_STREAMING_ERROR_DIALOG_DIFFERENT_NETWORK_TEXT,
          learn_more_link, &offset);
    } else {
      body_text = l10n_util::GetStringFUTF16(
          IDS_ASH_ECHE_APP_STREAMING_ERROR_DIALOG_UNSUPPORTED_NETWORK_TEXT,
          learn_more_link, &offset);
    }
    body_->SetText(body_text);

    views::StyledLabel::RangeStyleInfo style;
    style.override_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
    body_->AddStyleRange(gfx::Range(0, offset), style);

    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &ConnectionErrorDialogDelegateView::LearnMoreLinkPressed,
            base::Unretained(this),
            base::BindRepeating(
                &NewWindowDelegate::OpenUrl,
                base::Unretained(NewWindowDelegate::GetPrimary()),
                GURL(phonehub::kPhoneHubLearnMoreLink),
                NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                NewWindowDelegate::Disposition::kNewForegroundTab)));
    const SkColor link_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonLabelColorBlue);
    link_style.override_color = link_color;
    body_->AddStyleRange(gfx::Range(offset, offset + learn_more_link.length()),
                         link_style);

    body_->SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(kMarginBetweenTitleAndBody, 0,
                                         kMarginBetweenBodyAndButtons, 0));
    body_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
    body_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    body_->SetAutoColorReadabilityEnabled(false);

    body_->SetPaintToLayer();
    body_->layer()->SetFillsBoundsOpaquely(false);

    // TODO(b/254874005): Migrate the |body_| font to Google Sans. Use the same
    // TypographyProvider StyleLabel() but use ash::Typography::kCrosBody.

    // Add button row.
    auto* button_row = AddChildView(std::make_unique<views::View>());
    button_row
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            kMarginBetweenButtons))
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

    if (!is_on_different_network && !is_phone_on_cellular) {
      cancel_button_ =
          button_row->AddChildView(std::make_unique<ash::PillButton>(
              views::Button::PressedCallback(base::BindRepeating(
                  &ConnectionErrorDialogDelegateView::OnCancelClicked,
                  base::Unretained(this))),
              l10n_util::GetStringUTF16(
                  IDS_ASH_ECHE_APP_STREAMING_ERROR_DIALOG_DISMISS_TEXT),
              PillButton::Type::kDefaultWithoutIcon, nullptr));
      accept_button_ =
          button_row->AddChildView(std::make_unique<ash::PillButton>(
              views::Button::PressedCallback(base::BindRepeating(
                  &ConnectionErrorDialogDelegateView::OnStartTetheringClicked,
                  base::Unretained(this))),
              l10n_util::GetStringUTF16(
                  IDS_ASH_ECHE_APP_STREMING_ERROR_DIALOG_TURN_ON_HOTSPOT),
              PillButton::Type::kPrimaryWithoutIcon, nullptr));
    } else {
      cancel_button_ =
          button_row->AddChildView(std::make_unique<ash::PillButton>(
              views::Button::PressedCallback(base::BindRepeating(
                  &ConnectionErrorDialogDelegateView::OnCancelClicked,
                  base::Unretained(this))),
              l10n_util::GetStringUTF16(
                  IDS_ASH_ECHE_APP_STREAMING_ERROR_DIALOG_DISMISS_TEXT),
              PillButton::Type::kPrimaryWithoutIcon, nullptr));
    }
  }

  ConnectionErrorDialogDelegateView(const ConnectionErrorDialogDelegateView&) =
      delete;
  ConnectionErrorDialogDelegateView& operator=(
      const ConnectionErrorDialogDelegateView&) = delete;

  ~ConnectionErrorDialogDelegateView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(
        kDialogWidth,
        GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth));
  }

  void OnStartTetheringClicked(const ui::Event& event) {
    if (start_tethering_callback_) {
      std::move(start_tethering_callback_).Run(event);
    }

    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }

  void OnCancelClicked() {
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCancelButtonClicked);
  }

  void LearnMoreLinkPressed(base::RepeatingClosure callback) {
    std::move(callback).Run();
  }

 private:
  StartTetheringCallback start_tethering_callback_;
  std::unique_ptr<views::ViewShadow> view_shadow_;

  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::StyledLabel> body_ = nullptr;
  raw_ptr<views::Button> cancel_button_ = nullptr;
  raw_ptr<views::Button> accept_button_ = nullptr;
};

BEGIN_METADATA(ConnectionErrorDialogDelegateView)
END_METADATA

}  // namespace

AppStreamConnectionErrorDialog::AppStreamConnectionErrorDialog(
    views::View* host_view,
    base::OnceClosure on_close_callback,
    StartTetheringCallback button_callback,
    bool is_different_network,
    bool is_phone_one_cellular)
    : host_view_(host_view), on_close_callback_(std::move(on_close_callback)) {
  auto dialog = std::make_unique<ConnectionErrorDialogDelegateView>(
      std::move(button_callback), is_different_network, is_phone_one_cellular);
  views::Widget* const parent = host_view_->GetWidget();

  widget_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = parent->GetNativeWindow();
  params.delegate = dialog.release();

  widget_->Init(std::move(params));

  // The |dialog| ownership is passed to the window hierarchy.
  widget_observations_.AddObservation(widget_.get());
  widget_observations_.AddObservation(parent);

  view_observations_.AddObservation(host_view_.get());
  view_observations_.AddObservation(widget_->GetContentsView());
}

AppStreamConnectionErrorDialog::~AppStreamConnectionErrorDialog() {
  view_observations_.RemoveAllObservations();
  widget_observations_.RemoveAllObservations();
  if (widget_) {
    widget_->Close();
    widget_ = nullptr;
  }
}

void AppStreamConnectionErrorDialog::UpdateBounds() {
  if (!widget_) {
    return;
  }

  gfx::Point anchor_point_in_screen(host_view_->width() / 2,
                                    host_view_->height() / 2);
  views::View::ConvertPointToScreen(host_view_, &anchor_point_in_screen);

  gfx::Size dialog_size = widget_->GetContentsView()->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(
      gfx::Point(anchor_point_in_screen.x() - dialog_size.width() / 2,
                 anchor_point_in_screen.y() - dialog_size.height() / 2 -
                     kDialogVerticalOffset),
      dialog_size));
}

void AppStreamConnectionErrorDialog::OnWidgetDestroying(views::Widget* widget) {
  if (on_close_callback_) {
    std::move(on_close_callback_).Run();
  }
}

void AppStreamConnectionErrorDialog::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (widget == host_view_->GetWidget()) {
    UpdateBounds();
  }
}

void AppStreamConnectionErrorDialog::OnViewBoundsChanged(
    views::View* observed_view) {
  UpdateBounds();
}

void AppStreamConnectionErrorDialog::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  UpdateBounds();
}

}  // namespace ash
