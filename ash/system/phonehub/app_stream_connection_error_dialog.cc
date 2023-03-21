// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_connection_error_dialog.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "chromeos/ash/components/phonehub/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
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
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr int kDialogVerticalMargin = 50;

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
 public:
  explicit ConnectionErrorDialogDelegateView(
      StartTetheringCallback start_tethering_callback)
      : start_tethering_callback_(std::move(start_tethering_callback)) {
    SetModalType(ui::MODAL_TYPE_WINDOW);

    SetPaintToLayer();
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

    view_shadow_ = std::make_unique<ViewShadow>(this, kDialogShadowElevation);
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

    title_->SetPaintToLayer();
    title_->layer()->SetFillsBoundsOpaquely(false);

    // Add dialog body.
    const std::u16string learn_more_link =
        l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE);
    size_t offset;
    const std::u16string body_text = l10n_util::GetStringFUTF16(
        IDS_ASH_ECHE_APP_STREAMING_ERROR_DIALOG_MAIN_TEXT, learn_more_link,
        &offset);

    body_ = AddChildView(std::make_unique<views::StyledLabel>());
    body_->SetText(body_text);

    views::StyledLabel::RangeStyleInfo style;
    style.override_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
    body_->AddStyleRange(gfx::Range(0, offset), style);

    // TODO(b/273822975): Change Learn More link to a different page than the
    // default Phone Hub help page.
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

    // Add button row.
    auto* button_row = AddChildView(std::make_unique<views::View>());
    button_row
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            kMarginBetweenButtons))
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

    cancel_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
        views::Button::PressedCallback(base::BindRepeating(
            &ConnectionErrorDialogDelegateView::OnCancelClicked,
            base::Unretained(this))),
        l10n_util::GetStringUTF16(IDS_APP_CANCEL),
        PillButton::Type::kDefaultWithoutIcon, nullptr));
    accept_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
        views::Button::PressedCallback(base::BindRepeating(
            &ConnectionErrorDialogDelegateView::OnStartTetheringClicked,
            base::Unretained(this))),
        l10n_util::GetStringUTF16(
            IDS_ASH_ECHE_APP_STREAMING_ERROR_DIALOG_OK_TEXT),
        PillButton::Type::kPrimaryWithoutIcon, nullptr));
  }

  ConnectionErrorDialogDelegateView(const ConnectionErrorDialogDelegateView&) =
      delete;
  ConnectionErrorDialogDelegateView& operator=(
      const ConnectionErrorDialogDelegateView&) = delete;

  ~ConnectionErrorDialogDelegateView() override = default;

  // views::View:
  const char* GetClassName() const override {
    return "ConnectionErrorDialogDelegateView";
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
  }

  void OnThemeChanged() override {
    views::WidgetDelegateView::OnThemeChanged();

    SetBackground(views::CreateRoundedRectBackground(
        AshColorProvider::Get()->GetBaseLayerColor(
            AshColorProvider::BaseLayerType::kTransparent80),
        kDialogRoundedCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kDialogRoundedCornerRadius,
        views::HighlightBorder::Type::kHighlightBorder1,
        /*use_light_colors=*/false));
    title_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }

  void OnStartTetheringClicked() {
    if (start_tethering_callback_) {
      std::move(start_tethering_callback_).Run();
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
  std::unique_ptr<ViewShadow> view_shadow_;

  views::ImageView* icon_ = nullptr;
  views::Label* title_ = nullptr;
  views::StyledLabel* body_ = nullptr;
  views::Button* cancel_button_ = nullptr;
  views::Button* accept_button_ = nullptr;
};

}  // namespace

AppStreamConnectionErrorDialog::AppStreamConnectionErrorDialog(
    views::View* host_view,
    base::OnceClosure on_close_callback,
    StartTetheringCallback button_callback)
    : host_view_(host_view), on_close_callback_(std::move(on_close_callback)) {
  auto dialog = std::make_unique<ConnectionErrorDialogDelegateView>(
      std::move(button_callback));
  views::Widget* const parent = host_view_->GetWidget();

  widget_ = new views::Widget();
  views::Widget::InitParams params;

  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent = parent->GetNativeWindow();
  params.delegate = dialog.release();

  widget_->Init(std::move(params));

  // The |dialog| ownership is passed to the window hierarchy.
  widget_observations_.AddObservation(widget_);
  widget_observations_.AddObservation(parent);

  view_observations_.AddObservation(host_view_);
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

  gfx::Point anchor_point_in_screen(host_view_->width() / 2, 0);
  views::View::ConvertPointToScreen(host_view_, &anchor_point_in_screen);

  const int offset_for_frame_insets =
      widget_->non_client_view() && widget_->non_client_view()->frame_view()
          ? widget_->non_client_view()->frame_view()->GetInsets().top()
          : 0;
  const int vertical_offset = kDialogVerticalMargin - offset_for_frame_insets;

  gfx::Size dialog_size = widget_->GetContentsView()->GetPreferredSize();
  widget_->SetBounds(
      gfx::Rect(gfx::Point(anchor_point_in_screen.x() - dialog_size.width() / 2,
                           anchor_point_in_screen.y() + vertical_offset),
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