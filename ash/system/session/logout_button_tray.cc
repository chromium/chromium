// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_button_tray.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/user/login_status.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

LogoutButtonTray::LogoutButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kLogoutButton) {
  DCHECK(shelf);
  Shell::Get()->session_controller()->AddObserver(this);

  button_ =
      tray_container()->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&LogoutButtonTray::ButtonPressed,
                              base::Unretained(this)),
          std::u16string(), CONTEXT_LAUNCHER_BUTTON));
  button_->SetStyle(ui::ButtonStyle::kProminent);
  set_use_bounce_in_animation(false);

  SetFocusBehavior(FocusBehavior::NEVER);
}

LogoutButtonTray::~LogoutButtonTray() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void LogoutButtonTray::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShowLogoutButtonInTray, false);
  registry->RegisterIntegerPref(prefs::kLogoutDialogDurationMs, 20000);
}

void LogoutButtonTray::UpdateLayout() {
  // We must first update the button so that its container can lay it out
  // correctly.
  UpdateButtonTextAndImage();
  tray_container()->UpdateLayout();
}

void LogoutButtonTray::UpdateBackground() {
  // The logout button does not have a background.
}

void LogoutButtonTray::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  pref_change_registrar_.reset();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kShowLogoutButtonInTray,
      base::BindRepeating(&LogoutButtonTray::UpdateShowLogoutButtonInTray,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLogoutDialogDurationMs,
      base::BindRepeating(&LogoutButtonTray::UpdateLogoutDialogDuration,
                          base::Unretained(this)));

  // Read the initial values.
  UpdateShowLogoutButtonInTray();
  UpdateLogoutDialogDuration();
}

void LogoutButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  button_->SetBgColorIdOverride(cros_tokens::kColorAlert);
  button_->SetEnabledTextColors(
      color_provider->GetColor(cros_tokens::kColorPrimaryInverted));
}

void LogoutButtonTray::UpdateShowLogoutButtonInTray() {
  show_logout_button_in_tray_ = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kShowLogoutButtonInTray);
  UpdateVisibility();
}

void LogoutButtonTray::UpdateLogoutDialogDuration() {
  const int duration_ms = pref_change_registrar_->prefs()->GetInteger(
      prefs::kLogoutDialogDurationMs);
  dialog_duration_ = base::Milliseconds(duration_ms);
}

void LogoutButtonTray::UpdateAfterLoginStatusChange() {
  UpdateButtonTextAndImage();
}

void LogoutButtonTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {}

void LogoutButtonTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void LogoutButtonTray::HideBubble(const TrayBubbleView* bubble_view) {}

std::u16string LogoutButtonTray::GetAccessibleNameForTray() {
  return button_->GetText();
}

void LogoutButtonTray::HandleLocaleChange() {
  UpdateButtonTextAndImage();
}

void LogoutButtonTray::UpdateVisibility() {
  LoginStatus login_status = shelf()->GetStatusAreaWidget()->login_status();
  SetVisiblePreferred(show_logout_button_in_tray_ &&
                      login_status != LoginStatus::NOT_LOGGED_IN &&
                      login_status != LoginStatus::LOCKED);
}

void LogoutButtonTray::UpdateButtonTextAndImage() {
  LoginStatus login_status = shelf()->GetStatusAreaWidget()->login_status();
  const std::u16string title =
      user::GetLocalizedSignOutStringForStatus(login_status, false);
  if (shelf()->IsHorizontalAlignment()) {
    button_->SetText(title);
    button_->SetImageModel(views::Button::STATE_NORMAL, ui::ImageModel());
    button_->SetMinSize(gfx::Size(0, kTrayItemSize));
  } else {
    button_->SetText(std::u16string());
    button_->GetViewAccessibility().SetName(title);
    button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            kShelfLogoutIcon,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorPrimary)));
    button_->SetMinSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  }
  UpdateVisibility();
}

void LogoutButtonTray::ButtonPressed() {
  if (dialog_duration_ <= base::TimeDelta()) {
    if (Shell::Get()->session_controller()->IsDemoSession())
      base::RecordAction(base::UserMetricsAction("DemoMode.ExitFromShelf"));
    // Sign out immediately if |dialog_duration_| is non-positive.
    Shell::Get()->session_controller()->RequestSignOut();
  } else if (Shell::Get()->logout_confirmation_controller()) {
    Shell::Get()->logout_confirmation_controller()->ConfirmLogout(
        base::TimeTicks::Now() + dialog_duration_,
        LogoutConfirmationController::Source::kShelfExitButton);
  }
}

BEGIN_METADATA(LogoutButtonTray)
END_METADATA

}  // namespace ash
