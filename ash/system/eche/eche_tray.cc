// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "components/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Padding for tray icon (dp; the button that shows the eche menu).
constexpr int kTrayIconMainAxisInset = 6;
constexpr int kTrayIconCrossAxisInset = 0;

constexpr int kIconColumnWidth = 16;
constexpr int kIconWidth = 22;
constexpr int kIconHeight = 22;

constexpr gfx::Insets kBubblePadding(4, 4, kBubbleBottomPaddingDip, 4);

}  // namespace

EcheTray::EcheTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  icon_->SetTooltipText(GetAccessibleNameForTray());
  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
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

void EcheTray::OnThemeChanged() {
  // TODO(nayebi): Should we redraw the bubble?
  TrayBackgroundView::OnThemeChanged();
  // TODO(nayebi): Change this based on the final interaction model between the
  // phone hub and Eche components.
  icon_->SetImage(CreateVectorIcon(
      kPhoneHubPhoneIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
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

void EcheTray::OnSessionStateChanged(session_manager::SessionState state) {
  icon_->SetImage(CreateVectorIcon(kPhoneHubPhoneIcon, TrayIconColor(state)));
  // TODO(nayebi): Investigate if animations need to be stopped temporaily
}

void EcheTray::OnActiveUserSessionChanged(const AccountId& account_id) {
  // TODO(nayebi): Investigate if animations need to be stopped temporaily
}

void EcheTray::SetUrl(const GURL& url) {
  if (url_ != url)
    PurgeAndClose();
  url_ = url;
}

void EcheTray::SetIcon(const gfx::Image& icon) {
  icon_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      icon.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kIconWidth, kIconHeight)));
}

void EcheTray::PurgeAndClose() {
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
  init_params.anchor_rect = GetBubbleAnchor()->GetAnchorBoundsInScreen();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  // TODO(nayebi): get the width relative to the screen size
  init_params.preferred_width = 400;
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

  auto web_view = AshWebViewFactory::Get()->Create(AshWebView::InitParams());
  // TODO(nayebi): Use GetDefaultBoundsForEche()
  web_view->SetPreferredSize(gfx::Size(400, 600));
  if (!url_.is_empty())
    web_view->Navigate(url_);
  bubble_view->AddChildView(std::move(web_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view.release(),
                                                /*event_handling=*/false);

  SetIsActive(true);
  bubble_->GetBubbleView()->UpdateBubble();
}

void EcheTray::OnArrowBackActivated() {
  // TODO(nayebi): implement this
}

std::unique_ptr<views::View> EcheTray::CreateBubbleHeaderView() {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets(0, kIconColumnWidth));
  // TODO(nayebi) Set the right IDS for this button
  auto arrow_back_buttom = std::make_unique<IconButton>(
      base::BindRepeating(&EcheTray::OnArrowBackActivated,
                          weak_factory_.GetWeakPtr()),
      IconButton::Type::kSmall, &kSystemMenuArrowBackIcon,
      IDS_ASH_PHONE_HUB_CONNECTED_DEVICE_SETTINGS_LABEL);
  // TODO(nayebi): Make it visible when we are ready to handle this.
  // The theme of the button is also different from minimize and close
  // see screen/BZApR32ACmrtHZi
  arrow_back_buttom->SetVisible(false);
  header->AddChildView(arrow_back_buttom.release());

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

  auto minimize_button = views::BubbleFrameView::CreateMinimizeButton(
      base::BindRepeating(&EcheTray::CloseBubble, weak_factory_.GetWeakPtr()));

  minimize_button->SetProperty(views::kCrossAxisAlignmentKey,
                               views::LayoutAlignment::kStart);
  minimize_button->SetProperty(views::kInternalPaddingKey,
                               minimize_button->GetInsets());
  header->AddChildView(minimize_button.release());

  auto close_button =
      views::BubbleFrameView::CreateCloseButton(base::BindRepeating(
          &EcheTray::PurgeAndClose, weak_factory_.GetWeakPtr()));

  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
  // Set views::kInternalPaddingKey for flex layout to account for internal
  // button padding when calculating margins.
  close_button->SetProperty(views::kInternalPaddingKey,
                            close_button->GetInsets());
  header->AddChildView(close_button.release());

  return header;
}

BEGIN_METADATA(EcheTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
