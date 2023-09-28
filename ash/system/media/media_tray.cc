// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/focus_cycler.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/media_message_center/notification_theme.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
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

constexpr auto kTitleViewInsets = gfx::Insets::TLBR(0, 16, 0, 16);

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
    auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kTitleViewInsets));
    box_layout->set_minimum_cross_axis_size(kTitleViewHeight);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    title_label_ = AddChildView(std::make_unique<views::Label>());
    title_label_->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_GLOBAL_MEDIA_CONTROLS_TITLE));
    title_label_->SetAutoColorReadabilityEnabled(false);

    if (base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsCrOSUpdatedUI)) {
      title_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      if (chromeos::features::IsJellyEnabled()) {
        TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                              *title_label_);
      } else {
        TrayPopupUtils::SetLabelFontList(title_label_,
                                         TrayPopupUtils::FontStyle::kTitle);
      }
    } else {
      title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      title_label_->SetFontList(views::Label::GetDefaultFontList().Derive(
          kTitleFontSizeIncrease, gfx::Font::NORMAL,
          gfx::Font::Weight::MEDIUM));
    }

    // Media tray should always be pinned to shelf when we are opening the
    // dialog.
    DCHECK(MediaTray::IsPinnedToShelf());
    pin_button_ = AddChildView(std::make_unique<MediaTray::PinButton>());

    box_layout->SetFlexForView(title_label_, 1);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (!base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsCrOSUpdatedUI)) {
      SetBorder(views::CreatePaddedBorder(
          views::CreateSolidSidedBorder(
              gfx::Insets::TLBR(0, 0, kMenuSeparatorWidth, 0),
              AshColorProvider::Get()->GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kSeparatorColor)),
          gfx::Insets::TLBR(kMenuSeparatorVerticalPadding, 0,
                            kMenuSeparatorVerticalPadding - kMenuSeparatorWidth,
                            0)));
    }
    title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }

  views::Button* pin_button() { return pin_button_; }

 private:
  raw_ptr<views::ImageButton, ExperimentalAsh> pin_button_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;
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
      return GetIsPinnedToShelfByDefault();
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
          IconButton::Type::kMedium,
          &kUnpinnedIcon,
          MediaTray::IsPinnedToShelf()
              ? IDS_ASH_GLOBAL_MEDIA_CONTROLS_PINNED_BUTTON_TOOLTIP_TEXT
              : IDS_ASH_GLOBAL_MEDIA_CONTROLS_UNPINNED_BUTTON_TOOLTIP_TEXT,
          /*is_togglable=*/true,
          /*has_border=*/false) {
  SetIconSize(kTrayTopShortcutButtonIconSize);
  SetToggledVectorIcon(kPinnedIcon);
  if (chromeos::features::IsJellyEnabled()) {
    SetIconColorId(cros_tokens::kCrosSysOnSurface);
    SetBackgroundToggledColorId(cros_tokens::kCrosSysSystemPrimaryContainer);
  } else {
    SetIconColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorPrimary));
  }
  SetToggled(MediaTray::IsPinnedToShelf());
}

void MediaTray::PinButton::ButtonPressed() {
  MediaTray::SetPinnedToShelf(!MediaTray::IsPinnedToShelf());
  base::UmaHistogramBoolean("Media.CrosGlobalMediaControls.PinAction",
                            MediaTray::IsPinnedToShelf());

  SetToggled(MediaTray::IsPinnedToShelf());
  SetTooltipText(l10n_util::GetStringUTF16(
      MediaTray::IsPinnedToShelf()
          ? IDS_ASH_GLOBAL_MEDIA_CONTROLS_PINNED_BUTTON_TOOLTIP_TEXT
          : IDS_ASH_GLOBAL_MEDIA_CONTROLS_UNPINNED_BUTTON_TOOLTIP_TEXT));
}

MediaTray::MediaTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kMediaPlayer) {
  SetCallback(base::BindRepeating(&MediaTray::OnTrayButtonPressed,
                                  base::Unretained(this)));
  if (MediaNotificationProvider::Get()) {
    MediaNotificationProvider::Get()->AddObserver(this);
  }

  Shell::Get()->session_controller()->AddObserver(this);

  tray_container()->SetMargin(kMediaTrayPadding, 0);
  auto icon = std::make_unique<views::ImageView>();
  icon->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
  icon_ = tray_container()->AddChildView(std::move(icon));
  if (chromeos::features::IsJellyEnabled()) {
    UpdateTrayItemColor(is_active());
  } else {
    icon_->SetImage(ui::ImageModel::FromVectorIcon(kGlobalMediaControlsIcon,
                                                   kColorAshIconColorPrimary));
  }
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

void MediaTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void MediaTray::UpdateAfterLoginStatusChange() {
  UpdateDisplayState();
  PreferredSizeChanged();
}

void MediaTray::HandleLocaleChange() {
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
}

views::Widget* MediaTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

TrayBubbleView* MediaTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

void MediaTray::ShowBubble() {
  ShowBubbleWithItem("");
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

void MediaTray::UpdateTrayItemColor(bool is_active) {
  DCHECK(chromeos::features::IsJellyEnabled());
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kGlobalMediaControlsIcon,
      is_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                : cros_tokens::kCrosSysOnSurface));
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

void MediaTray::OnTrayButtonPressed() {
  if (GetBubbleWidget()) {
    CloseBubble();
    return;
  }

  ShowBubble();
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

void MediaTray::ShowBubbleWithItem(const std::string& item_id) {
  DCHECK(MediaNotificationProvider::Get());
  SetNotificationColorTheme();

  std::unique_ptr<TrayBubbleView> bubble_view =
      std::make_unique<TrayBubbleView>(CreateInitParamsForTrayBubble(this));

  auto* title_view = bubble_view->AddChildView(
      std::make_unique<GlobalMediaControlsTitleView>());
  title_view->SetPaintToLayer();
  title_view->layer()->SetFillsBoundsOpaquely(false);
  pin_button_ = title_view->pin_button();

  content_view_ = bubble_view->AddChildView(
      MediaNotificationProvider::Get()->GetMediaNotificationListView(
          kMenuSeparatorWidth, /*should_clip_height=*/true, item_id));
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsCrOSUpdatedUI)) {
    content_view_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, 0, kMediaNotificationListViewBottomPadding, 0)));
  }

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));
  SetIsActive(true);

  base::UmaHistogramBoolean("Media.CrosGlobalMediaControls.RepeatUsageOnShelf",
                            bubble_has_shown_);
  bubble_has_shown_ = true;
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
  theme.background_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
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

}  // namespace ash
