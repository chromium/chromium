// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/focus_cycler.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/media_message_center/notification_theme.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr int kNoMediaTextFontSizeIncrease = 2;
constexpr int kTitleFontSizeIncrease = 4;
constexpr int kTitleViewHeight = 56;

constexpr gfx::Insets kTitleViewInsets = gfx::Insets(0, 16, 0, 16);

// Minimum screen diagonal (in inches) for pinning global media controls
// on shelf by default.
constexpr float kMinimumScreenSizeDiagonal = 10.0f;

// Calculate screen size and returns true if screen size is larger than
// kMinimumScreenSizeDiagonal.
bool GetIsPinnedToShelfByDefault() {
  // Happens in test.
  if (!Shell::HasInstance())
    return false;

  display::ManagedDisplayInfo info =
      Shell::Get()->display_manager()->GetDisplayInfo(
          display::Screen::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(info.device_dpi());
  float screen_width = info.size_in_pixel().width() / info.device_dpi();
  float screen_height = info.size_in_pixel().height() / info.device_dpi();

  float diagonal_len = sqrt(pow(screen_width, 2) + pow(screen_height, 2));
  return diagonal_len > kMinimumScreenSizeDiagonal;
}

// Used for getting default pin state for experiment.
bool GetIsPinnedToShelfByFeatureParams() {
  switch (media::kCrosGlobalMediaControlsPinParam.Get()) {
    case media::kCrosGlobalMediaControlsPinOptions::kPin:
      return true;
    case media::kCrosGlobalMediaControlsPinOptions::kNotPin:
      return false;
    case media::kCrosGlobalMediaControlsPinOptions::kHeuristic:
      return GetIsPinnedToShelfByDefault();
  }

  NOTREACHED();
  return false;
}

// Enum that specifies the pin state of global media controls.
enum PinState {
  kDefault = 0,
  kUnpinned,
  kPinned,
};

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
        views::BoxLayout::Orientation::kHorizontal, kTitleViewInsets));
    box_layout->set_minimum_cross_axis_size(kTitleViewHeight);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* title_label = AddChildView(std::make_unique<views::Label>());
    title_label->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_GLOBAL_MEDIA_CONTROLS_TITLE));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    title_label->SetAutoColorReadabilityEnabled(false);
    title_label->SetFontList(views::Label::GetDefaultFontList().Derive(
        kTitleFontSizeIncrease, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

    // Media tray should always be pinned to shelf when we are opening the
    // dialog.
    DCHECK(MediaTray::IsPinnedToShelf());
    pin_button_ = AddChildView(std::make_unique<MediaTray::PinButton>());

    box_layout->SetFlexForView(title_label, 1);
  }

  views::Button* pin_button() { return pin_button_; }

 private:
  MediaTray::PinButton* pin_button_ = nullptr;
};

}  // namespace

// static
void MediaTray::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kGlobalMediaControlsPinned,
                                PinState::kDefault);
}

// static
bool MediaTray::IsPinnedToShelf() {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(pref_service);
  switch (pref_service->GetInteger(prefs::kGlobalMediaControlsPinned)) {
    case PinState::kPinned:
      return true;
    case PinState::kUnpinned:
      return false;
    case PinState::kDefault:
      return GetIsPinnedToShelfByFeatureParams();
  }

  NOTREACHED();
  return false;
}

// static
void MediaTray::SetPinnedToShelf(bool pinned) {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(pref_service);
  pref_service->SetInteger(prefs::kGlobalMediaControlsPinned,
                           pinned ? PinState::kPinned : PinState::kUnpinned);
}

MediaTray::PinButton::PinButton()
    : IconButton(
          base::BindRepeating(&PinButton::ButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kSmall,
          MediaTray::IsPinnedToShelf() ? &kPinnedIcon : &kUnpinnedIcon,
          MediaTray::IsPinnedToShelf()
              ? IDS_ASH_GLOBAL_MEDIA_CONTROLS_PINNED_BUTTON_TOOLTIP_TEXT
              : IDS_ASH_GLOBAL_MEDIA_CONTROLS_UNPINNED_BUTTON_TOOLTIP_TEXT) {}

void MediaTray::PinButton::ButtonPressed() {
  MediaTray::SetPinnedToShelf(!MediaTray::IsPinnedToShelf());
  base::UmaHistogramBoolean("Media.CrosGlobalMediaControls.PinAction",
                            MediaTray::IsPinnedToShelf());

  SetImage(views::Button::STATE_NORMAL,
           CreateVectorIcon(
               MediaTray::IsPinnedToShelf() ? kPinnedIcon : kUnpinnedIcon,
               kTrayTopShortcutButtonIconSize,
               AshColorProvider::Get()->GetContentLayerColor(
                   AshColorProvider::ContentLayerType::kIconColorPrimary)));
  SetTooltipText(l10n_util::GetStringUTF16(
      MediaTray::IsPinnedToShelf()
          ? IDS_ASH_GLOBAL_MEDIA_CONTROLS_PINNED_BUTTON_TOOLTIP_TEXT
          : IDS_ASH_GLOBAL_MEDIA_CONTROLS_UNPINNED_BUTTON_TOOLTIP_TEXT));
}

MediaTray::MediaTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  if (MediaNotificationProvider::Get())
    MediaNotificationProvider::Get()->AddObserver(this);

  Shell::Get()->session_controller()->AddObserver(this);

  auto icon = std::make_unique<views::ImageView>();
  icon->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
  tray_container()->SetMargin(kMediaTrayPadding, 0);
  icon_ = tray_container()->AddChildView(std::move(icon));
}

MediaTray::~MediaTray() {
  if (bubble_)
    bubble_->GetBubbleView()->ResetDelegate();

  if (MediaNotificationProvider::Get())
    MediaNotificationProvider::Get()->RemoveObserver(this);

  Shell::Get()->session_controller()->RemoveObserver(this);
}

void MediaTray::OnNotificationListChanged() {
  UpdateDisplayState();
}

void MediaTray::OnNotificationListViewSizeChanged() {
  if (!bubble_)
    return;

  bubble_->GetBubbleView()->UpdateBubble();
}

std::u16string MediaTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT);
}

void MediaTray::UpdateAfterLoginStatusChange() {
  UpdateDisplayState();
  PreferredSizeChanged();
}

void MediaTray::HandleLocaleChange() {
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
}

bool MediaTray::PerformAction(const ui::Event& event) {
  if (bubble_)
    CloseBubble();
  else
    ShowBubble();
  return true;
}

void MediaTray::ShowBubble() {
  DCHECK(MediaNotificationProvider::Get());
  SetNotificationColorTheme();

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = GetAnchorBoundsInScreen();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.reroute_event_handler = true;

  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);

  auto* title_view = bubble_view->AddChildView(
      std::make_unique<GlobalMediaControlsTitleView>());
  title_view->SetPaintToLayer();
  title_view->layer()->SetFillsBoundsOpaquely(false);
  pin_button_ = title_view->pin_button();

  content_view_ = bubble_view->AddChildView(
      MediaNotificationProvider::Get()->GetMediaNotificationListView(
          kMenuSeparatorWidth));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view);
  SetIsActive(true);

  base::UmaHistogramBoolean("Media.CrosGlobalMediaControls.RepeatUsageOnShelf",
                            bubble_has_shown_);
  bubble_has_shown_ = true;
}

void MediaTray::CloseBubble() {
  if (MediaNotificationProvider::Get())
    MediaNotificationProvider::Get()->OnBubbleClosing();
  SetIsActive(false);
  empty_state_view_ = nullptr;
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

void MediaTray::OnLockStateChanged(bool locked) {
  UpdateDisplayState();
}

void MediaTray::OnActiveUserPrefServiceChanged(PrefService* pref_service) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kGlobalMediaControlsPinned,
      base::BindRepeating(&MediaTray::OnGlobalMediaControlsPinPrefChanged,
                          base::Unretained(this)));
  OnGlobalMediaControlsPinPrefChanged();
}

void MediaTray::UpdateDisplayState() {
  if (!MediaNotificationProvider::Get())
    return;

  if (bubble_ && Shell::Get()->session_controller()->IsScreenLocked())
    CloseBubble();

  bool has_session =
      MediaNotificationProvider::Get()->HasActiveNotifications() ||
      MediaNotificationProvider::Get()->HasFrozenNotifications();

  if (bubble_ && !has_session)
    ShowEmptyState();

  if (bubble_ && has_session && empty_state_view_)
    empty_state_view_->SetVisible(false);

  bool should_show = has_session &&
                     !Shell::Get()->session_controller()->IsScreenLocked() &&
                     IsPinnedToShelf();

  SetVisiblePreferred(should_show);
}

std::u16string MediaTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_GLOBAL_MEDIA_CONTROLS_TITLE);
}

void MediaTray::SetNotificationColorTheme() {
  if (!MediaNotificationProvider::Get())
    return;

  media_message_center::NotificationTheme theme;
  theme.primary_text_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  theme.secondary_text_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
  theme.enabled_icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  theme.disabled_icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorSecondary);
  theme.separator_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  MediaNotificationProvider::Get()->SetColorTheme(theme);
}

void MediaTray::OnGlobalMediaControlsPinPrefChanged() {
  UpdateDisplayState();
}

void MediaTray::ShowEmptyState() {
  DCHECK(content_view_);
  if (empty_state_view_) {
    empty_state_view_->SetVisible(true);
    return;
  }

  // Create and add empty state view containing a label indicating there's no
  // active session
  auto empty_state_view = std::make_unique<views::View>();
  auto* layout =
      empty_state_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_minimum_cross_axis_size(content_view_->bounds().height());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  auto no_media_label = std::make_unique<views::Label>();
  no_media_label->SetAutoColorReadabilityEnabled(false);
  no_media_label->SetSubpixelRenderingEnabled(false);
  no_media_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  no_media_label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_GLOBAL_MEDIA_CONTROLS_NO_MEDIA_TEXT));
  no_media_label->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(
          kNoMediaTextFontSizeIncrease));
  empty_state_view->AddChildView(std::move(no_media_label));

  empty_state_view->SetPaintToLayer();
  empty_state_view->layer()->SetFillsBoundsOpaquely(false);
  empty_state_view_ =
      bubble_->GetBubbleView()->AddChildView(std::move(empty_state_view));
}

void MediaTray::AnchorUpdated() {
  if (!bubble_)
    return;

  bubble_->GetBubbleView()->SetAnchorRect(
      shelf()->GetStatusAreaWidget()->GetMediaTrayAnchorRect());
}

void MediaTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  icon_->SetImage(gfx::CreateVectorIcon(
      kGlobalMediaControlsIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
}

}  // namespace ash
