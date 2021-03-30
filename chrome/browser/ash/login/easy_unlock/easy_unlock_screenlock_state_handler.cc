// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_screenlock_state_handler.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

using proximity_auth::ScreenlockState;

namespace chromeos {

namespace {

proximity_auth::ScreenlockBridge::UserPodCustomIcon GetIconForState(
    ScreenlockState state) {
  switch (state) {
    case ScreenlockState::NO_BLUETOOTH:
    case ScreenlockState::NO_PHONE:
    case ScreenlockState::PHONE_NOT_AUTHENTICATED:
    case ScreenlockState::PHONE_LOCKED:
    case ScreenlockState::PHONE_NOT_LOCKABLE:
      return proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED;
    case ScreenlockState::RSSI_TOO_LOW:
    case ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW:
      // TODO(isherman): This icon is currently identical to the regular locked
      // icon.  Once the reduced proximity range flag is removed, consider
      // deleting the redundant icon.
      return proximity_auth::ScreenlockBridge::
          USER_POD_CUSTOM_ICON_LOCKED_WITH_PROXIMITY_HINT;
    case ScreenlockState::BLUETOOTH_CONNECTING:
      return proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_SPINNER;
    case ScreenlockState::AUTHENTICATED:
      return proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_UNLOCKED;
    case ScreenlockState::INACTIVE:
      return proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_NONE;
    case ScreenlockState::PASSWORD_REAUTH:
      return proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_HARDLOCKED;
    case ScreenlockState::PRIMARY_USER_ABSENT:
      return proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED;
  }

  NOTREACHED();
  return proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_NONE;
}

bool HardlockOnClick(ScreenlockState state) {
  return state != ScreenlockState::INACTIVE;
}

size_t GetTooltipResourceId(ScreenlockState state) {
  switch (state) {
    case ScreenlockState::INACTIVE:
    case ScreenlockState::BLUETOOTH_CONNECTING:
      return 0;
    case ScreenlockState::NO_BLUETOOTH:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_BLUETOOTH;
    case ScreenlockState::NO_PHONE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_PHONE;
    case ScreenlockState::PHONE_NOT_AUTHENTICATED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_NOT_AUTHENTICATED;
    case ScreenlockState::PHONE_LOCKED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_LOCKED;
    case ScreenlockState::PHONE_NOT_LOCKABLE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_UNLOCKABLE;
    case ScreenlockState::RSSI_TOO_LOW:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_RSSI_TOO_LOW;
    case ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_LOCKED_AND_RSSI_TOO_LOW;
    case ScreenlockState::AUTHENTICATED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_INSTRUCTIONS;
    case ScreenlockState::PASSWORD_REAUTH:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PASSWORD_REAUTH;
    case ScreenlockState::PRIMARY_USER_ABSENT:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PRIMARY_USER_ABSENT;
  }

  NOTREACHED();
  return 0;
}

bool TooltipContainsDeviceType(ScreenlockState state) {
  return (state == ScreenlockState::AUTHENTICATED ||
          state == ScreenlockState::PHONE_NOT_LOCKABLE ||
          state == ScreenlockState::NO_BLUETOOTH ||
          state == ScreenlockState::RSSI_TOO_LOW ||
          state == ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW);
}

// Returns true iff the `state` corresponds to a locked remote device.
bool IsLockedState(ScreenlockState state) {
  return (state == ScreenlockState::PHONE_LOCKED ||
          state == ScreenlockState::PHONE_LOCKED_AND_RSSI_TOO_LOW);
}

}  // namespace

EasyUnlockScreenlockStateHandler::EasyUnlockScreenlockStateHandler(
    const AccountId& account_id,
    HardlockState initial_hardlock_state,
    proximity_auth::ScreenlockBridge* screenlock_bridge,
    proximity_auth::ProximityAuthPrefManager* pref_manager)
    : state_(ScreenlockState::INACTIVE),
      account_id_(account_id),
      screenlock_bridge_(screenlock_bridge),
      pref_manager_(pref_manager),
      hardlock_state_(initial_hardlock_state) {
  DCHECK(screenlock_bridge_);
  screenlock_bridge_->AddObserver(this);
}

EasyUnlockScreenlockStateHandler::~EasyUnlockScreenlockStateHandler() {
  screenlock_bridge_->RemoveObserver(this);
  // Make sure the screenlock state set by this gets cleared.
  ChangeState(ScreenlockState::INACTIVE);
}

bool EasyUnlockScreenlockStateHandler::IsActive() const {
  return state_ != ScreenlockState::INACTIVE;
}

bool EasyUnlockScreenlockStateHandler::InStateValidOnRemoteAuthFailure() const {
  // Note that NO_PHONE is not valid in this case because the phone may close
  // the connection if the auth challenge sent to it is invalid. This case
  // should be handled as authentication failure.
  return state_ == ScreenlockState::INACTIVE ||
         state_ == ScreenlockState::NO_BLUETOOTH ||
         state_ == ScreenlockState::PHONE_LOCKED;
}

void EasyUnlockScreenlockStateHandler::ChangeState(ScreenlockState new_state) {
  if (state_ == new_state)
    return;

  state_ = new_state;

  // If lock screen is not active or it forces offline password, just cache the
  // current state. The screenlock state will get refreshed in `ScreenDidLock`.
  if (!screenlock_bridge_->IsLocked())
    return;

  // Do nothing when auth type is online.
  if (screenlock_bridge_->lock_handler()->GetAuthType(account_id_) ==
      proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
    return;
  }

  if (IsLockedState(state_))
    did_see_locked_phone_ = true;

  if (hardlock_state_ != NO_HARDLOCK) {
    ShowHardlockUI();
    return;
  }

  UpdateScreenlockAuthType();

  proximity_auth::ScreenlockBridge::UserPodCustomIcon icon =
      GetIconForState(state_);

  if (icon == proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_NONE) {
    screenlock_bridge_->lock_handler()->HideUserPodCustomIcon(account_id_);
    return;
  }

  proximity_auth::ScreenlockBridge::UserPodCustomIconOptions icon_options;
  icon_options.SetIcon(icon);

  if (HardlockOnClick(state_))
    icon_options.SetHardlockOnClick();

  UpdateTooltipOptions(&icon_options);

  // For states without tooltips, we still need to set an accessibility label.
  if (state_ == ScreenlockState::BLUETOOTH_CONNECTING) {
    icon_options.SetAriaLabel(
        l10n_util::GetStringUTF16(IDS_SMART_LOCK_SPINNER_ACCESSIBILITY_LABEL));
  }

  // Accessibility users may not be able to see the green icon which indicates
  // the phone is authenticated. Provide message to that effect.
  if (state_ == ScreenlockState::AUTHENTICATED) {
    icon_options.SetAriaLabel(l10n_util::GetStringUTF16(
        IDS_SMART_LOCK_SCREENLOCK_AUTHENTICATED_LABEL));
  }

  screenlock_bridge_->lock_handler()->ShowUserPodCustomIcon(account_id_,
                                                            icon_options);
}

void EasyUnlockScreenlockStateHandler::SetHardlockState(
    HardlockState new_state) {
  if (hardlock_state_ == new_state)
    return;

  if (new_state == LOGIN_FAILED && hardlock_state_ != NO_HARDLOCK)
    return;

  hardlock_state_ = new_state;

  // If hardlock_state_ was set to NO_HARDLOCK, this means the screen is about
  // to get unlocked. No need to update it in this case.
  if (hardlock_state_ != NO_HARDLOCK) {
    hardlock_ui_shown_ = false;

    RefreshScreenlockState();
  }
}

void EasyUnlockScreenlockStateHandler::MaybeShowHardlockUI() {
  if (hardlock_state_ != NO_HARDLOCK)
    ShowHardlockUI();
}

void EasyUnlockScreenlockStateHandler::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  did_see_locked_phone_ = IsLockedState(state_);
  RefreshScreenlockState();
}

void EasyUnlockScreenlockStateHandler::OnScreenDidUnlock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  if (hardlock_state_ == LOGIN_FAILED)
    hardlock_state_ = NO_HARDLOCK;
  hardlock_ui_shown_ = false;

  // Upon a successful unlock event, record whether the user's phone was locked
  // at any point while the lock screen was up.
  if (state_ == ScreenlockState::AUTHENTICATED)
    RecordEasyUnlockDidUserManuallyUnlockPhone(did_see_locked_phone_);
  did_see_locked_phone_ = false;
}

void EasyUnlockScreenlockStateHandler::OnFocusedUserChanged(
    const AccountId& account_id) {}

void EasyUnlockScreenlockStateHandler::RefreshScreenlockState() {
  ScreenlockState last_state = state_;
  // This should force updating screenlock state.
  state_ = ScreenlockState::INACTIVE;
  ChangeState(last_state);
}

void EasyUnlockScreenlockStateHandler::ShowHardlockUI() {
  DCHECK(hardlock_state_ != NO_HARDLOCK);

  if (!screenlock_bridge_->IsLocked())
    return;

  // Do not override online signin.
  const proximity_auth::mojom::AuthType existing_auth_type =
      screenlock_bridge_->lock_handler()->GetAuthType(account_id_);
  if (existing_auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN)
    return;

  if (existing_auth_type != proximity_auth::mojom::AuthType::OFFLINE_PASSWORD) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        account_id_, proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
        std::u16string());
  }

  if (hardlock_state_ == NO_PAIRING) {
    screenlock_bridge_->lock_handler()->HideUserPodCustomIcon(account_id_);
    hardlock_ui_shown_ = false;
    return;
  }

  if (hardlock_ui_shown_)
    return;

  proximity_auth::ScreenlockBridge::UserPodCustomIconOptions icon_options;
  if (hardlock_state_ == LOGIN_FAILED) {
    icon_options.SetIcon(
        proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED);
  } else if (hardlock_state_ == PAIRING_CHANGED ||
             hardlock_state_ == PAIRING_ADDED) {
    icon_options.SetIcon(proximity_auth::ScreenlockBridge::
                             USER_POD_CUSTOM_ICON_LOCKED_TO_BE_ACTIVATED);
  } else {
    icon_options.SetIcon(
        proximity_auth::ScreenlockBridge::USER_POD_CUSTOM_ICON_HARDLOCKED);
  }

  std::u16string device_name = GetDeviceName();
  std::u16string tooltip;
  switch (hardlock_state_) {
    case USER_HARDLOCK:
      tooltip = l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_USER, device_name);
      break;
    case PAIRING_CHANGED:
      tooltip = l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_PAIRING_CHANGED,
          device_name);
      break;
    case PAIRING_ADDED:
      tooltip = l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_PAIRING_ADDED,
          device_name);
      break;
    case LOGIN_FAILED:
      tooltip = l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_LOGIN_FAILURE);
      break;
    case LOGIN_DISABLED:
      tooltip = l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_LOGIN_DISABLED);
      break;
    default:
      LOG(ERROR) << "Unknown hardlock state: " << hardlock_state_;
      NOTREACHED();
  }

  bool autoshow = true;
  // TODO(crbug.com/1152491): Only call into SetHasShownLoginDisabledMessage()
  // if this is a signin screen, not lock screen, context.
  if (hardlock_state_ == LOGIN_DISABLED) {
    // If Signin with Smart Lock is disabled, only automatically show the
    // tooltip if it hasn't been shown yet. See https://crbug.com/848893 for
    // details.
    autoshow = !pref_manager_->HasShownLoginDisabledMessage();
    pref_manager_->SetHasShownLoginDisabledMessage(true);
  }

  icon_options.SetTooltip(tooltip, autoshow);

  screenlock_bridge_->lock_handler()->ShowUserPodCustomIcon(account_id_,
                                                            icon_options);
  hardlock_ui_shown_ = true;
}

void EasyUnlockScreenlockStateHandler::UpdateTooltipOptions(
    proximity_auth::ScreenlockBridge::UserPodCustomIconOptions* icon_options) {
  size_t resource_id = 0;
  std::u16string device_name;
  resource_id = GetTooltipResourceId(state_);
  if (TooltipContainsDeviceType(state_))
    device_name = GetDeviceName();

  if (!resource_id)
    return;

  std::u16string tooltip;
  if (device_name.empty()) {
    tooltip = l10n_util::GetStringUTF16(resource_id);
  } else {
    tooltip = l10n_util::GetStringFUTF16(resource_id, device_name);
  }

  if (tooltip.empty())
    return;

  bool autoshow_tooltip = state_ != ScreenlockState::AUTHENTICATED;
  icon_options->SetTooltip(tooltip, autoshow_tooltip);
}

std::u16string EasyUnlockScreenlockStateHandler::GetDeviceName() {
  return ui::GetChromeOSDeviceName();
}

void EasyUnlockScreenlockStateHandler::UpdateScreenlockAuthType() {
  if (hardlock_state_ != NO_HARDLOCK)
    return;

  // Do not override online signin.
  const proximity_auth::mojom::AuthType existing_auth_type =
      screenlock_bridge_->lock_handler()->GetAuthType(account_id_);
  DCHECK_NE(proximity_auth::mojom::AuthType::ONLINE_SIGN_IN,
            existing_auth_type);

  if (state_ == ScreenlockState::AUTHENTICATED) {
    if (existing_auth_type != proximity_auth::mojom::AuthType::USER_CLICK) {
      screenlock_bridge_->lock_handler()->SetAuthType(
          account_id_, proximity_auth::mojom::AuthType::USER_CLICK,
          l10n_util::GetStringUTF16(
              IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE));
    }
  } else if (existing_auth_type !=
             proximity_auth::mojom::AuthType::OFFLINE_PASSWORD) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        account_id_, proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
        std::u16string());
  }
}

}  // namespace chromeos
