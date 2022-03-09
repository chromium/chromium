// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_tray.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
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
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
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

// Padding for tray icon (dp; the button that shows the eche menu).
constexpr int kTrayIconMainAxisInset = 6;
constexpr int kTrayIconCrossAxisInset = 0;

constexpr int kIconColumnWidth = 16;
constexpr int kIconWidth = 22;
constexpr int kIconHeight = 22;

constexpr gfx::Insets kBubblePadding(4, 4, kBubbleBottomPaddingDip, 4);

constexpr float kDefaultAspectRatio = 16.0 / 9.0f;
constexpr int kMinimumEcheWidth = 240;

}  // namespace

EcheTray::EcheTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  observed_session_.Observe(Shell::Get()->session_controller());
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

  auto web_view = AshWebViewFactory::Get()->Create(AshWebView::InitParams());
  web_view->SetPreferredSize(GetSizeForEche());
  if (!url_.is_empty())
    web_view->Navigate(url_);
  web_view_ = bubble_view->AddChildView(std::move(web_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view.release(),
                                                /*event_handling=*/false);

  SetIsActive(true);
  bubble_->GetBubbleView()->UpdateBubble();
}

gfx::Size EcheTray::GetSizeForEche() const {
  // Ensures the Eche bounds is always 16:9 portrait aspect ratio and not more
  // than half of the windows.
  gfx::Rect bounds = display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(
                             tray_container()->GetWidget()->GetNativeWindow())
                         .work_area();
  const float bounds_aspect_ratio =
      static_cast<float>(bounds.width()) / bounds.height();
  const bool is_landscape = bounds_aspect_ratio >= 1;
  int new_width = is_landscape ? (bounds.height() / 2) : (bounds.width() / 2);
  new_width = std::min(new_width, kMinimumEcheWidth);
  return gfx::Size(new_width, new_width * kDefaultAspectRatio);
}

void EcheTray::OnArrowBackActivated() {
  if (web_view_)
    web_view_->GoBack();
}

std::unique_ptr<views::View> EcheTray::CreateBubbleHeaderView() {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets(0, kIconColumnWidth));
  auto arrow_back_buttom = CreateArrowBackButton(base::BindRepeating(
      &EcheTray::OnArrowBackActivated, weak_factory_.GetWeakPtr()));
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

std::unique_ptr<views::Button> EcheTray::CreateArrowBackButton(
    Button::PressedCallback callback) {
  auto arrow_back_button = views::CreateVectorImageButtonWithNativeTheme(
      std::move(callback), vector_icons::kBackArrowIcon);
  arrow_back_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_BACK));
  arrow_back_button->SizeToPreferredSize();

  views::InstallCircleHighlightPathGenerator(arrow_back_button.get());

  return arrow_back_button;
}

BEGIN_METADATA(EcheTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
