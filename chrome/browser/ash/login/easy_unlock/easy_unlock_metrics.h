// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_METRICS_H_

namespace base {
class TimeDelta;
}

namespace ash {

// Tracking login events for Easy unlock metrics.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum EasyUnlockAuthEvent {
  // User is successfully authenticated using Easy Unlock.
  EASY_UNLOCK_SUCCESS = 0,
  // Easy Unlock failed to authenticate the user.
  EASY_UNLOCK_FAILURE = 1,

  // Password is used because there is no pairing data.
  PASSWORD_ENTRY_NO_PAIRING = 2,
  // Password is used because pairing data is changed.
  PASSWORD_ENTRY_PAIRING_CHANGED = 3,
  // Password is used because of user hardlock.
  PASSWORD_ENTRY_USER_HARDLOCK = 4,
  // Password is used because Easy unlock service is not active.
  PASSWORD_ENTRY_SERVICE_NOT_ACTIVE = 5,
  // Password is used because Bluetooth is not on.
  PASSWORD_ENTRY_NO_BLUETOOTH = 6,
  // Password is used because Easy unlock is connecting.
  PASSWORD_ENTRY_BLUETOOTH_CONNECTING = 7,
  // Password is used because no eligible phones found.
  PASSWORD_ENTRY_NO_PHONE = 8,
  // Password is used because phone could not be authenticated.
  PASSWORD_ENTRY_PHONE_NOT_AUTHENTICATED = 9,
  // Password is used because phone is locked.
  PASSWORD_ENTRY_PHONE_LOCKED = 10,
  // Password is used because phone does not have lock screen.
  PASSWORD_ENTRY_PHONE_NOT_LOCKABLE = 11,
  // Password is used because phone is not close enough (roughly, at least 30
  // feet away).
  PASSWORD_ENTRY_RSSI_TOO_LOW = 12,
  // Deprecated. Password is used because phone is not supported.
  DEPRECATED_PASSWORD_ENTRY_PHONE_UNSUPPORTED = 13,
  // Password is used because user types in password. This is unlikely to happen
  // though.
  PASSWORD_ENTRY_WITH_AUTHENTICATED_PHONE = 14,
  // Password is used because phone is not right next to the Chromebook.
  PASSWORD_ENTRY_TX_POWER_TOO_HIGH = 15,  // DEPRECATED
  // Password is used because Easy sign-in failed.
  PASSWORD_ENTRY_LOGIN_FAILED = 16,
  // Password is used because pairing data is changed for a "new" Chromebook
  // (where there was no previous pairing data).
  PASSWORD_ENTRY_PAIRING_ADDED = 17,
  // Password is used because there is no smartlock state handler. Most likely
  // because EasyUnlock is disabled, e.g. Bluetooth adapter not ready.
  PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER = 18,
  // Password is used because the phone is (a) locked, and (b) not right next to
  // the Chromebook.
  PASSWORD_ENTRY_PHONE_LOCKED_AND_RSSI_TOO_LOW = 19,

  // Password entry was forced due to the reauth policy (e.g. the user must type
  // their password every 20 hours).
  PASSWORD_ENTRY_FORCED_REAUTH = 20,

  // Password entry was forced because sign-in with Smart Lock is disabled.
  PASSWORD_ENTRY_LOGIN_DISABLED = 21,

  // Password is used because primary user was in background or user is
  // secondary user.
  PASSWORD_ENTRY_PRIMARY_USER_ABSENT = 22,

  EASY_UNLOCK_AUTH_EVENT_COUNT  // Must be the last entry.
};

void RecordEasyUnlockDidUserManuallyUnlockPhone(bool did_unlock);
void RecordEasyUnlockSigninDuration(const base::TimeDelta& duration);
void RecordEasyUnlockSigninEvent(EasyUnlockAuthEvent event);
void RecordEasyUnlockScreenUnlockDuration(const base::TimeDelta& duration);
void RecordEasyUnlockScreenUnlockEvent(EasyUnlockAuthEvent event);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_METRICS_H_
