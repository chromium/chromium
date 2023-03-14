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
#include "ash/system/power/adaptive_charging_controller.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/buttons.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/power_button.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr gfx::Insets kQuickSettingFooterPadding(16);
constexpr int kQuickSettingFooterItemBetweenSpacing = 8;

}  // namespace

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
  button_container_layout->set_between_child_spacing(16);

  power_button_ =
      front_buttons_container->AddChildView(std::make_unique<PowerButton>());
  if (Shell::Get()->session_controller()->login_status() !=
      LoginStatus::NOT_LOGGED_IN) {
    auto* user_avatar_button = front_buttons_container->AddChildView(
        std::make_unique<UserAvatarButton>(base::BindRepeating(
            [](UnifiedSystemTrayController* controller) {
              quick_settings_metrics_util::RecordQsButtonActivated(
                  QsButtonCatalogName::kAvatarButton);
              controller->ShowUserChooserView();
            },
            base::Unretained(controller))));
    user_avatar_button->SetEnabled(
        UserChooserDetailedViewController::IsUserChooserEnabled());
    user_avatar_button->SetID(VIEW_ID_QS_USER_AVATAR_BUTTON);
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
      AddChildView(std::make_unique<BatteryIconView>(controller));
    }
    AddChildView(
        std::make_unique<BatteryLabelView>(controller, use_smart_charging_ui));
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
