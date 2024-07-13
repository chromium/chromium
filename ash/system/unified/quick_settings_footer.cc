// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_footer.h"

#include <cstddef>
#include <memory>
#include <numeric>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/power/adaptive_charging_controller.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/power_button.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/system/unified/user_chooser_view.h"
#include "ash/system/user/login_status.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr gfx::Insets kQuickSettingFooterPadding(16);
constexpr int kQuickSettingFooterItemBetweenSpacing = 8;
constexpr int kImageLabelSpacing = 2;
constexpr int kHorizontalSpacing = 12;
constexpr int kPaddingReduction = 0;

bool ShouldShowSignOutButton() {
  auto* session_controller = Shell::Get()->session_controller();
  // Don't show before login.
  if (!session_controller->IsActiveUserSessionStarted()) {
    return false;
  }
  // Show "Exit guest" or "Exit session" button for special account types.
  if (session_controller->IsUserGuest() ||
      session_controller->IsUserPublicAccount()) {
    return true;
  }

  // More than one user logged in.
  if (session_controller->NumberOfLoggedInUsers() > 1) {
    // In this state, UX wants to only show the user avatar button.
    return false;
  }

  std::optional<int> number_of_users_that_could_be_logged_in =
      session_controller->GetExistingUsersCount();
  const bool multiple_past_accounts =
      number_of_users_that_could_be_logged_in.has_value() &&
      number_of_users_that_could_be_logged_in.value() > 1;

  // Show the sign out button if only one account is logged in, but multiple
  // are on the device.
  return multiple_past_accounts;
}

bool ShouldShowAvatar() {
  // Only show the avatar if there are multiple logged in users, and we are
  // logged in.
  return Shell::Get()->session_controller()->login_status() !=
             LoginStatus::NOT_LOGGED_IN &&
         Shell::Get()->session_controller()->NumberOfLoggedInUsers() > 1;
}

// The avatar button shows in the quick setting bubble.
class UserAvatarButton : public views::Button {
  METADATA_HEADER(UserAvatarButton, views::Button)

 public:
  explicit UserAvatarButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetBorder(views::CreateEmptyBorder(gfx::Insets(0)));
    AddChildView(CreateUserAvatarView(/*user_index=*/0));
    SetTooltipText(GetUserItemAccessibleString(/*user_index=*/0));
    SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
    views::InstallCircleHighlightPathGenerator(this);
  }

  UserAvatarButton(const UserAvatarButton&) = delete;

  UserAvatarButton& operator=(const UserAvatarButton&) = delete;

  ~UserAvatarButton() override = default;
};

BEGIN_METADATA(UserAvatarButton)
END_METADATA

}  // namespace

QsBatteryInfoViewBase::QsBatteryInfoViewBase(
    UnifiedSystemTrayController* controller,
    const Type type,
    gfx::VectorIcon* icon)
    : PillButton(base::BindRepeating(
                     [](UnifiedSystemTrayController* controller) {
                       quick_settings_metrics_util::RecordQsButtonActivated(
                           QsButtonCatalogName::kBatteryButton);
                       controller->HandleOpenPowerSettingsAction();
                     },
                     // TODO(crbug.com/40061562): Remove
                     // `UnsafeDanglingUntriaged`
                     base::UnsafeDanglingUntriaged(controller)),
                 PowerStatus::Get()->GetInlinedStatusString(),
                 type,
                 icon,
                 kHorizontalSpacing,
                 kPaddingReduction) {
  PowerStatus::Get()->AddObserver(this);
  SetImageLabelSpacing(kImageLabelSpacing);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                        *label());

  GetViewAccessibility().SetName(
      PowerStatus::Get()->GetAccessibleNameString(/*full_description=*/true));
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
}

QsBatteryInfoViewBase::~QsBatteryInfoViewBase() {
  PowerStatus::Get()->RemoveObserver(this);
}

void QsBatteryInfoViewBase::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void QsBatteryInfoViewBase::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

// PowerStatus::Observer:
void QsBatteryInfoViewBase::OnPowerStatusChanged() {
  Update();
}

void QsBatteryInfoViewBase::OnThemeChanged() {
  PillButton::OnThemeChanged();
  Update();
}

void QsBatteryInfoViewBase::UpdateIconAndText(bool bsm_active) {
  // Change to icon type if battery saver mode is enabled with
  // QsBatteryLabelView.
  SetPillButtonType(Type::kPrimaryWithIconLeading);
  SetButtonTextColorId(bsm_active
                           ? cros_tokens::kCrosSysSystemOnWarningContainer
                           : cros_tokens::kCrosSysOnPositiveContainer);
  SetBackgroundColorId(bsm_active ? cros_tokens::kCrosSysSystemWarningContainer
                                  : cros_tokens::kCrosSysPositiveContainer);

  const std::u16string percentage_text =
      PowerStatus::Get()->GetStatusStrings().first;
  SetText(percentage_text);
  GetViewAccessibility().SetName(
      PowerStatus::Get()->GetAccessibleNameString(/*full_description=*/true));
  SetVisible(!percentage_text.empty());

  if (GetColorProvider()) {
    ConfigureIcon(bsm_active);
  }
}

void QsBatteryInfoViewBase::ConfigureIcon(bool bsm_active) {
  const SkColor battery_icon_color = GetColorProvider()->GetColor(
      bsm_active ? cros_tokens::kCrosSysSystemOnWarningContainer
                 : cros_tokens::kCrosSysOnPositiveContainer);
  const std::optional<SkColor> battery_badge_color =
      bsm_active ? std::optional<SkColor>(GetColorProvider()->GetColor(
                       cros_tokens::kCrosSysSystemWarningContainer))
                 : std::nullopt;

  PowerStatus::BatteryImageInfo info =
      PowerStatus::Get()->GenerateBatteryImageInfo(battery_icon_color,
                                                   battery_badge_color);
  info.alert_if_low = false;
  if (bsm_active) {
    // Use solid battery icon for battery saver mode enabled.
    info.charge_percent = 100;
  }

  SetImageModel(ButtonState::STATE_NORMAL,
                ui::ImageModel::FromImageSkia(PowerStatus::GetBatteryImage(
                    info, kUnifiedTrayBatteryIconSize, GetColorProvider())));
}

BEGIN_METADATA(QsBatteryInfoViewBase)
END_METADATA

QsBatteryLabelView::QsBatteryLabelView(UnifiedSystemTrayController* controller)
    : QsBatteryInfoViewBase(controller) {
  SetID(VIEW_ID_QS_BATTERY_BUTTON);
  views::FocusRing::Get(/*host=*/this)
      ->SetColorId(cros_tokens::kCrosSysFocusRing);
  Update();
}

QsBatteryLabelView::~QsBatteryLabelView() = default;

void QsBatteryLabelView::Update() {
  if (PowerStatus::Get()->IsBatterySaverActive()) {
    UpdateIconAndText(true);
  } else {
    SetButtonTextColorId(cros_tokens::kCrosSysOnSurface);

    const std::u16string status_string =
        PowerStatus::Get()->GetInlinedStatusString();
    SetText(status_string);
    SetVisible(!status_string.empty());
  }
  GetViewAccessibility().SetName(
      PowerStatus::Get()->GetAccessibleNameString(/*full_description=*/true));
}

BEGIN_METADATA(QsBatteryLabelView)
END_METADATA

QsBatteryIconView::QsBatteryIconView(UnifiedSystemTrayController* controller)
    : QsBatteryInfoViewBase(controller, Type::kPrimaryWithIconLeading) {
  SetID(VIEW_ID_QS_BATTERY_BUTTON);
  // Sets the text and icon.
  Update();
}

QsBatteryIconView::~QsBatteryIconView() = default;

void QsBatteryIconView::Update() {
  UpdateIconAndText(PowerStatus::Get()->IsBatterySaverActive());
}

BEGIN_METADATA(QsBatteryIconView)
END_METADATA

QuickSettingsFooter::QuickSettingsFooter(
    UnifiedSystemTrayController* controller) {
  DCHECK(controller);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kQuickSettingFooterPadding,
      kQuickSettingFooterItemBetweenSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* front_buttons_container = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* button_container_layout =
      front_buttons_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal));
  button_container_layout->set_between_child_spacing(8);

  power_button_ = front_buttons_container->AddChildView(
      std::make_unique<PowerButton>(controller));

  if (ShouldShowAvatar()) {
    auto* user_avatar_button = front_buttons_container->AddChildView(
        std::make_unique<UserAvatarButton>(base::BindRepeating(
            [](UnifiedSystemTrayController* controller) {
              quick_settings_metrics_util::RecordQsButtonActivated(
                  QsButtonCatalogName::kAvatarButton);
              controller->ShowUserChooserView();
            },
            // TODO(crbug.com/40061562): Remove `UnsafeDanglingUntriaged`
            base::UnsafeDanglingUntriaged(controller))));
    user_avatar_button->SetEnabled(
        UserChooserDetailedViewController::IsUserChooserEnabled());
    user_avatar_button->SetID(VIEW_ID_QS_USER_AVATAR_BUTTON);
  }

  if (ShouldShowSignOutButton()) {
    sign_out_button_ =
        front_buttons_container->AddChildView(std::make_unique<PillButton>(
            base::BindRepeating(
                [](UnifiedSystemTrayController* controller) {
                  quick_settings_metrics_util::RecordQsButtonActivated(
                      QsButtonCatalogName::kSignOutButton);
                  controller->HandleSignOutAction();
                },
                // TODO(crbug.com/40061562): Remove
                // `UnsafeDanglingUntriaged`
                base::UnsafeDanglingUntriaged(controller)),
            user::GetLocalizedSignOutStringForStatus(
                Shell::Get()->session_controller()->login_status(),
                /*multiline=*/false),
            PillButton::Type::kDefaultWithoutIcon,
            /*icon=*/nullptr));
    sign_out_button_->SetID(VIEW_ID_QS_SIGN_OUT_BUTTON);
  }

  // `PowerButton` should start aligned , also battery icons and
  // `settings_button_` should be end aligned, so here adding a empty spacing
  // flex occupying all remaining space.
  auto* spacing = AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(spacing, 1);

  views::View* end_container = nullptr;
  if (PowerStatus::Get()->IsBatteryPresent()) {
    end_container = CreateEndContainer();
    const bool use_smart_charging_ui =
        ash::features::IsAdaptiveChargingEnabled() &&
        Shell::Get()
            ->adaptive_charging_controller()
            ->is_adaptive_delaying_charge();

    if (use_smart_charging_ui) {
      end_container->AddChildView(
          std::make_unique<QsBatteryIconView>(controller));
    } else {
      end_container->AddChildView(
          std::make_unique<QsBatteryLabelView>(controller));
    }
  }

  if (TrayPopupUtils::CanOpenWebUISettings()) {
    if (!end_container) {
      end_container = CreateEndContainer();
    }
    settings_button_ = end_container->AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(
            [](UnifiedSystemTrayController* controller) {
              quick_settings_metrics_util::RecordQsButtonActivated(
                  QsButtonCatalogName::kSettingsButton);
              controller->HandleSettingsAction();
            },
            base::Unretained(controller)),
        IconButton::Type::kMedium, &vector_icons::kSettingsOutlineIcon,
        IDS_ASH_STATUS_TRAY_SETTINGS));
    settings_button_->SetID(VIEW_ID_QS_SETTINGS_BUTTON);
    settings_button_->SetProperty(views::kElementIdentifierKey,
                                  kQuickSettingsSettingsButtonElementId);

    local_state_pref_change_registrar_.Init(Shell::Get()->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kOsSettingsEnabled,
        base::BindRepeating(&QuickSettingsFooter::UpdateSettingsButtonState,
                            base::Unretained(this)));
    UpdateSettingsButtonState();
  }
}

QuickSettingsFooter::~QuickSettingsFooter() = default;

// static
void QuickSettingsFooter::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOsSettingsEnabled, true);
}

void QuickSettingsFooter::UpdateSettingsButtonState() {
  PrefService* const local_state = Shell::Get()->local_state();
  const bool settings_icon_enabled =
      local_state->GetBoolean(prefs::kOsSettingsEnabled);

  settings_button_->SetState(settings_icon_enabled
                                 ? views::Button::STATE_NORMAL
                                 : views::Button::STATE_DISABLED);
}

views::View* QuickSettingsFooter::CreateEndContainer() {
  auto* end_container = AddChildView(std::make_unique<views::View>());
  auto* end_container_layout =
      end_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kQuickSettingFooterItemBetweenSpacing));
  end_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  return end_container;
}

BEGIN_METADATA(QuickSettingsFooter)
END_METADATA

}  // namespace ash
