// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/public/cpp/media_notification_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_utils.h"
#include "base/strings/string_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// View that contains global media controls' title.
class GlobalMediaControlsTitleView : public views::View {
 public:
  GlobalMediaControlsTitleView() {
    SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(
            0, 0, kMenuSeparatorWidth, 0,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kSeparatorColor)),
        gfx::Insets(kMenuSeparatorVerticalPadding, 0,
                    kMenuSeparatorVerticalPadding - kMenuSeparatorWidth, 0)));

    auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0, 0, 0)));
    box_layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);

    auto* title_label = AddChildView(std::make_unique<views::Label>());
    title_label->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_GLOBAL_MEDIA_CONTROLS_TITLE));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetBorder(views::CreateEmptyBorder(0, 16, 0, 0));
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SMALL_TITLE,
                             true /* use_unified_theme */);
    style.SetupLabel(title_label);

    box_layout->SetFlexForView(title_label, 1);
  }
};

}  // namespace

MediaTray::MediaTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  DCHECK(MediaNotificationProvider::Get());
  MediaNotificationProvider::Get()->AddObserver(this);

  auto icon = std::make_unique<views::ImageView>();
  icon->set_tooltip_text(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
  icon->SetImage(gfx::CreateVectorIcon(
      kGlobalMediaControlsIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));

  tray_container()->SetMargin(kMediaTrayPadding, 0);
  icon_ = tray_container()->AddChildView(std::move(icon));
}

MediaTray::~MediaTray() {
  if (bubble_)
    bubble_->GetBubbleView()->ResetDelegate();

  if (MediaNotificationProvider::Get())
    MediaNotificationProvider::Get()->RemoveObserver(this);
}

void MediaTray::OnNotificationListChanged() {
  UpdateDisplayState();
}

void MediaTray::OnNotificationListViewSizeChanged() {
  if (!bubble_)
    return;

  bubble_->GetBubbleView()->UpdateBubble();
}

base::string16 MediaTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT);
}

void MediaTray::UpdateAfterLoginStatusChange() {
  UpdateDisplayState();
  PreferredSizeChanged();
}

void MediaTray::HandleLocaleChange() {
  icon_->set_tooltip_text(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
}

bool MediaTray::PerformAction(const ui::Event& event) {
  if (bubble_)
    CloseBubble();
  else
    ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
  return true;
}

void MediaTray::ShowBubble(bool show_by_click) {
  DCHECK(MediaNotificationProvider::Get());

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = shelf()->GetSystemTrayAnchorRect();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.show_by_click = show_by_click;

  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);

  auto* title_view_ptr = bubble_view->AddChildView(
      std::make_unique<GlobalMediaControlsTitleView>());
  title_view_ptr->SetPaintToLayer();
  title_view_ptr->layer()->SetFillsBoundsOpaquely(false);

  bubble_view->AddChildView(
      MediaNotificationProvider::Get()->GetMediaNotificationListView(
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kSeparatorColor),
          kMenuSeparatorWidth));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view,
                                                false /*is_persistent*/);
  SetIsActive(true);
}

void MediaTray::CloseBubble() {
  if (MediaNotificationProvider::Get())
    MediaNotificationProvider::Get()->OnBubbleClosing();
  SetIsActive(false);
  bubble_.reset();
  shelf()->UpdateAutoHideState();
}

void MediaTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_ && bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void MediaTray::ClickedOutsideBubble() {
  CloseBubble();
}

void MediaTray::UpdateDisplayState() {
  if (!MediaNotificationProvider::Get())
    return;

  bool should_show =
      MediaNotificationProvider::Get()->HasActiveNotifications() ||
      MediaNotificationProvider::Get()->HasFrozenNotifications();

  if (!should_show && bubble_)
    CloseBubble();
  SetVisiblePreferred(should_show);
}

}  // namespace ash
