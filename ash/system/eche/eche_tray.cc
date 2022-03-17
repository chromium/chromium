// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_tray.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "components/account_id/account_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The icon size should be smaller than the tray item size to avoid the icon
// padding becoming negative.
constexpr int kIconSize = 22;

constexpr int kHeaderHeight = 40;
constexpr int kHeaderHorizontalInteriorMargins = 12;
constexpr gfx::Insets kHeaderDefaultSpacing =
    gfx::Insets(/*vertical=*/0, /*horizontal=*/8);

constexpr gfx::Insets kBubblePadding(/*vertical=*/8, /*horizontal=*/8);

constexpr float kDefaultAspectRatio = 16.0 / 9.0f;
constexpr gfx::Size kDefaultBubbleSize(360, 360 * kDefaultAspectRatio);

// Max percentage of the screen height that can be covered by the eche bubble.
constexpr float kMaxHeightPercentage = 0.85;

// Creates a button with the given callback, icon, and tooltip text.
// `message_id` is the resource id of the tooltip text of the icon.
std::unique_ptr<views::Button> CreateButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int message_id) {
  SkColor color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  auto button = views::CreateVectorImageButton(std::move(callback));
  views::SetImageFromVectorIconWithColor(button.get(), icon, color);
  button->SetTooltipText(l10n_util::GetStringUTF16(message_id));
  button->SizeToPreferredSize();

  views::InstallCircleHighlightPathGenerator(button.get());

  return button;
}

}  // namespace

EcheTray::EcheTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  observed_session_.Observe(Shell::Get()->session_controller());

  const int icon_padding = (kTrayItemSize - kIconSize) / 2;

  icon_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(icon_padding, icon_padding)));

  icon_->SetTooltipText(GetAccessibleNameForTray());

  if (features::IsEcheSWAInBackgroundEnabled()) {
    loading_indicator_ = icon_->AddChildView(
        std::make_unique<EcheIconLoadingIndicatorView>(icon_));
    loading_indicator_->SetVisible(false);
  }
}

EcheTray::~EcheTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
}

void EcheTray::ClickedOutsideBubble() {
  //  Do nothing
}

std::u16string EcheTray::GetAccessibleNameForTray() {
  // TODO(nayebi): Change this based on the final model of interaction
  // between phone hub and Eche.
  return l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME);
}

void EcheTray::HandleLocaleChange() {
  icon_->SetTooltipText(GetAccessibleNameForTray());
}

void EcheTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    HideBubble();
}

void EcheTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void EcheTray::Initialize() {
  TrayBackgroundView::Initialize();

  // By default the icon is not visible until Eche notification is clicked on.
  SetVisiblePreferred(false);
}

void EcheTray::CloseBubble() {
  if (bubble_)
    HideBubble();
}

void EcheTray::ShowBubble() {
  if (bubble_) {
    bubble_->GetBubbleWidget()->Show();
    bubble_->GetBubbleWidget()->Activate();
    bubble_->bubble_view()->SetVisible(true);
    SetIsActive(true);

    // TODO(b/223297066): Observe the connection status and add/remove the
    // loading indicator based on the connection status.
    if (features::IsEcheSWAInBackgroundEnabled() &&
        loading_indicator_->GetAnimating()) {
      loading_indicator_->SetAnimating(false);
    }
    return;
  }

  InitBubble();

  // TODO(nayebi): Add metric updates.
}

bool EcheTray::PerformAction(const ui::Event& event) {
  // Simply toggle between visible/invisibvle
  if (bubble_ && bubble_->bubble_view()->GetVisible()) {
    HideBubble();
  } else {
    ShowBubble();
  }
  return true;
}

TrayBubbleView* EcheTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* EcheTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

std::u16string EcheTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool EcheTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void EcheTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void EcheTray::OnLockStateChanged(bool locked) {
  if (bubble_ && locked)
    PurgeAndClose();
}

void EcheTray::SetUrl(const GURL& url) {
  if (url_ != url)
    PurgeAndClose();
  url_ = url;
}

void EcheTray::SetIcon(const gfx::Image& icon) {
  icon_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      icon.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kIconSize, kIconSize)));
}

void EcheTray::PurgeAndClose() {
  if (features::IsEcheSWAInBackgroundEnabled() &&
      loading_indicator_->GetAnimating()) {
    loading_indicator_->SetAnimating(false);
  }

  if (!bubble_)
    return;

  auto* bubble_view = bubble_->GetBubbleView();
  if (bubble_view)
    bubble_view->ResetDelegate();

  bubble_.reset();
  SetIsActive(false);
  SetVisiblePreferred(false);
}

void EcheTray::HideBubble() {
  SetIsActive(false);
  bubble_->bubble_view()->SetVisible(false);
  bubble_->GetBubbleWidget()->Deactivate();
  bubble_->GetBubbleWidget()->Hide();
}

void EcheTray::InitBubble() {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = shelf()->GetSystemTrayAnchorRect();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = GetSizeForEche().width();
  init_params.close_on_deactivate = false;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.reroute_event_handler = false;
  init_params.corner_radius = kTrayItemCornerRadius;

  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  bubble_view->SetCanActivate(true);
  bubble_view->SetBorder(views::CreateEmptyBorder(kBubblePadding));

  auto* header_view = bubble_view->AddChildView(CreateBubbleHeaderView());
  // The layer is needed to draw the header non-opaquely that is needed to
  // match the phone hub behavior.
  header_view->SetPaintToLayer();
  header_view->layer()->SetFillsBoundsOpaquely(false);

  AshWebView::InitParams params;
  params.can_record_media = true;
  auto web_view = AshWebViewFactory::Get()->Create(params);
  web_view->SetPreferredSize(GetSizeForEche());
  if (!url_.is_empty())
    web_view->Navigate(url_);
  web_view_ = bubble_view->AddChildView(std::move(web_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view.release(),
                                                /*event_handling=*/false);

  SetIsActive(true);
  bubble_->GetBubbleView()->UpdateBubble();

  if (features::IsEcheSWAInBackgroundEnabled())
    loading_indicator_->SetAnimating(true);
}

gfx::Size EcheTray::GetSizeForEche() const {
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              tray_container()->GetWidget()->GetNativeWindow())
          .work_area();
  float height_scale =
      (static_cast<float>(work_area_bounds.height()) * kMaxHeightPercentage) /
      kDefaultBubbleSize.height();
  height_scale = std::min(height_scale, 1.0f);
  return gfx::ScaleToFlooredSize(kDefaultBubbleSize, height_scale);
}

void EcheTray::OnArrowBackActivated() {
  if (web_view_)
    web_view_->GoBack();
}

std::unique_ptr<views::View> EcheTray::CreateBubbleHeaderView() {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(
          gfx::Insets(/*vertical=*/0,
                      /*horizontal=*/kHeaderHorizontalInteriorMargins))
      .SetCollapseMargins(true)
      .SetMinimumCrossAxisSize(kHeaderHeight)
      .SetDefault(views::kMarginsKey, kHeaderDefaultSpacing)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Add arrowback button
  header->AddChildView(
      CreateButton(base::BindRepeating(&EcheTray::OnArrowBackActivated,
                                       weak_factory_.GetWeakPtr()),
                   kEcheArrowBackIcon, IDS_APP_ACCNAME_BACK));

  views::Label* title = header->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  title->SetMultiLine(true);
  title->SetAllowCharacterBreak(true);
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Add minimize button
  header->AddChildView(CreateButton(
      base::BindRepeating(&EcheTray::CloseBubble, weak_factory_.GetWeakPtr()),
      kEcheMinimizeIcon, IDS_APP_ACCNAME_MINIMIZE));

  // Add close button
  header->AddChildView(CreateButton(
      base::BindRepeating(&EcheTray::PurgeAndClose, weak_factory_.GetWeakPtr()),
      kEcheCloseIcon, IDS_APP_ACCNAME_CLOSE));

  return header;
}

BEGIN_METADATA(EcheTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
