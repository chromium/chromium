// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_footer.h"

#include <cstddef>
#include <memory>
#include <numeric>

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
#include "ash/system/unified/buttons.h"
#include "ash/system/unified/power_button.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/user/login_status.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

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
  // For regular accounts, only show if there is more than one account on the
  // device.
  absl::optional<int> user_count = session_controller->GetExistingUsersCount();
  return user_count.has_value() && user_count.value() > 1;
}

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
                     controller),
                 PowerStatus::Get()->GetInlinedStatusString(),
                 type,
                 icon,
                 kHorizontalSpacing,
                 kPaddingReduction) {
  PowerStatus::Get()->AddObserver(this);
  SetImageLabelSpacing(kImageLabelSpacing);
  if (chromeos::features::IsJellyEnabled()) {
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label());
    return;
  }
  SetUseDefaultLabelFont();
}

QsBatteryInfoViewBase::~QsBatteryInfoViewBase() {
  PowerStatus::Get()->RemoveObserver(this);
}

void QsBatteryInfoViewBase::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kLabelText;
  node_data->SetName(
      PowerStatus::Get()->GetAccessibleNameString(/*full_description=*/true));
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

QsBatteryLabelView::QsBatteryLabelView(UnifiedSystemTrayController* controller)
    : QsBatteryInfoViewBase(controller) {
  SetID(VIEW_ID_QS_BATTERY_BUTTON);
  views::FocusRing::Get(/*host=*/this)
      ->SetColorId(cros_tokens::kCrosSysFocusRing);

  // Sets the text.
  Update();
}

QsBatteryLabelView::~QsBatteryLabelView() = default;

void QsBatteryLabelView::OnThemeChanged() {
  PillButton::OnThemeChanged();

  SetButtonTextColorId(cros_tokens::kCrosSysOnSurface);
}

void QsBatteryLabelView::Update() {
  const std::u16string status_string =
      PowerStatus::Get()->GetInlinedStatusString();
  SetText(status_string);
  SetVisible(!status_string.empty());
}

QsBatteryIconView::QsBatteryIconView(UnifiedSystemTrayController* controller)
    : QsBatteryInfoViewBase(controller, Type::kPrimaryWithIconLeading) {
  SetID(VIEW_ID_QS_BATTERY_BUTTON);

  // Sets the text and icon.
  Update();
}

QsBatteryIconView::~QsBatteryIconView() = default;

void QsBatteryIconView::OnThemeChanged() {
  PillButton::OnThemeChanged();

  SetButtonTextColorId(cros_tokens::kCrosSysOnPositiveContainer);
  SetBackgroundColorId(cros_tokens::kCrosSysPositiveContainer);
  ConfigureIcon();
}

void QsBatteryIconView::Update() {
  const std::u16string percentage_text =
      PowerStatus::Get()->GetStatusStrings().first;
  SetText(percentage_text);
  SetVisible(!percentage_text.empty());

  if (GetColorProvider()) {
    ConfigureIcon();
  }
}

void QsBatteryIconView::ConfigureIcon() {
  const SkColor battery_icon_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysOnPositiveContainer);

  PowerStatus::BatteryImageInfo info =
      PowerStatus::Get()->GetBatteryImageInfo();
  info.alert_if_low = false;

  SetImageModel(ButtonState::STATE_NORMAL,
                ui::ImageModel::FromImageSkia(PowerStatus::GetBatteryImage(
                    info, kUnifiedTrayBatteryIconSize, battery_icon_color)));
}

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

  if (ShouldShowSignOutButton()) {
    sign_out_button_ =
        front_buttons_container->AddChildView(std::make_unique<PillButton>(
            base::BindRepeating(
                [](UnifiedSystemTrayController* controller) {
                  quick_settings_metrics_util::RecordQsButtonActivated(
                      QsButtonCatalogName::kSignOutButton);
                  controller->HandleSignOutAction();
                },
                base::Unretained(controller)),
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

  if (PowerStatus::Get()->IsBatteryPresent()) {
    const bool use_smart_charging_ui =
        ash::features::IsAdaptiveChargingEnabled() &&
        Shell::Get()
            ->adaptive_charging_controller()
            ->is_adaptive_delaying_charge();

    if (use_smart_charging_ui) {
      AddChildView(std::make_unique<QsBatteryIconView>(controller));
    } else {
      AddChildView(std::make_unique<QsBatteryLabelView>(controller));
    }
  }

  if (TrayPopupUtils::CanOpenWebUISettings()) {
    settings_button_ = AddChildView(std::make_unique<IconButton>(
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

BEGIN_METADATA(QuickSettingsFooter, views::View)
END_METADATA

}  // namespace ash
