// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_view.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

HpsNotifyView::HpsNotifyView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();
  SetVisible(false);

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();

  OnSessionStateChanged(session_controller->GetSessionState());
  session_observation_.Observe(session_controller);

  // Poll the current preference state if the pref service is already loaded for
  // an active user. Then, from now on, observe changes to the active user pref
  // service.
  const AccountId& account_id = session_controller->GetActiveAccountId();
  PrefService* pref_service =
      session_controller->GetUserPrefServiceForUser(account_id);
  OnActiveUserPrefServiceChanged(pref_service);

  // Poll the current HPS notify state if the daemon is active. Then, from now
  // on, observe changes to the HPS notify signal.
  chromeos::HpsDBusClient::Get()->GetResultHpsNotify(base::BindOnce(
      &HpsNotifyView::OnHpsPollResult, weak_ptr_factory_.GetWeakPtr()));
  hps_dbus_observation_.Observe(chromeos::HpsDBusClient::Get());
}

HpsNotifyView::~HpsNotifyView() = default;

// static
void HpsNotifyView::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kSnoopingProtectionEnabled,
      /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void HpsNotifyView::HandleLocaleChange() {}

void HpsNotifyView::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  UpdateIconColor(session_state);
  UpdateIconVisibility(session_state == session_manager::SessionState::OOBE,
                       hps_state_, is_enabled_);
}

void HpsNotifyView::OnActiveUserPrefServiceChanged(PrefService* pref_service) {
  UpdateIconVisibility(is_oobe_, hps_state_,
                       pref_service && pref_service->GetBoolean(
                                           prefs::kSnoopingProtectionEnabled));

  if (!pref_service)
    return;

  // Re-subscribe to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionEnabled,
      base::BindRepeating(&HpsNotifyView::OnPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

void HpsNotifyView::OnHpsNotifyChanged(bool hps_state) {
  UpdateIconVisibility(is_oobe_, hps_state, is_enabled_);
}

void HpsNotifyView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateIconColor(Shell::Get()->session_controller()->GetSessionState());
}

const char* HpsNotifyView::GetClassName() const {
  return "HpsNotifyView";
}

void HpsNotifyView::UpdateIconColor(
    session_manager::SessionState session_state) {
  const SkColor new_color = TrayIconColor(session_state);
  const gfx::ImageSkia new_icon = gfx::CreateVectorIcon(gfx::IconDescription(
      kSystemTrayHpsNotifyIcon, kUnifiedTrayIconSize, new_color));
  image_view()->SetImage(new_icon);
}

void HpsNotifyView::UpdateIconVisibility(bool is_oobe,
                                         bool hps_state,
                                         bool is_enabled) {
  if (is_oobe_ == is_oobe && hps_state_ == hps_state &&
      is_enabled_ == is_enabled)
    return;

  is_oobe_ = is_oobe;
  hps_state_ = hps_state;
  is_enabled_ = is_enabled;

  SetVisible(!is_oobe_ && hps_state_ && is_enabled);
}

void HpsNotifyView::OnHpsPollResult(absl::optional<bool> result) {
  if (!result.has_value()) {
    LOG(WARNING) << "Polling the presence daemon failed";
    return;
  }

  UpdateIconVisibility(is_oobe_, *result, is_enabled_);
}

void HpsNotifyView::OnPrefChanged() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());

  UpdateIconVisibility(is_oobe_, hps_state_,
                       pref_change_registrar_->prefs()->GetBoolean(
                           prefs::kSnoopingProtectionEnabled));
}

}  // namespace ash
