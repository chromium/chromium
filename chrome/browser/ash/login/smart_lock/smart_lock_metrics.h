// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_METRICS_H_

namespace base {
class TimeDelta;
}

namespace ash {

// Tracking login events for Smart Lock metrics.
// This enum is used to define the buckets for an enumerated UMA histogram. Those UMA histograms use the deprecated "EasyUnlock" name to refer to Smart Lock. 
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum SmartLockAuthEvent {
  // User is successfully authenticated using Smart Lock.
  SMART_LOCK_SUCCESS = 0,
  // Smart Lock failed to authenticate the user.
  SMART_LOCK_FAILURE = 1,

  // (Deprecated) Password is used because there is no pairing data.
  // PASSWORD_ENTRY_NO_PAIRING = 2,
  // (Deprecated) Password is used because pairing data is changed.
  // PASSWORD_ENTRY_PAIRING_CHANGED = 3,
  // (Deprecated) Password is used because of user hardlock.
  // PASSWORD_ENTRY_USER_HARDLOCK = 4,
  // Password is used because Smart Lock service is not active.
  PASSWORD_ENTRY_SERVICE_NOT_ACTIVE = 5,
  // Password is used because Bluetooth is not on.
  PASSWORD_ENTRY_NO_BLUETOOTH = 6,
  // Password is used because Smart Lock is connecting.
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
  // (Deprecated) Password is used because phone is not supported.
  // DEPRECATED_PASSWORD_ENTRY_PHONE_UNSUPPORTED = 13,
  // Password is used because user types in password. This is unlikely to happen
  // though.
  PASSWORD_ENTRY_WITH_AUTHENTICATED_PHONE = 14,
  // (Deprecated) Password is used because phone is not right next to the Chromebook.
  // PASSWORD_ENTRY_TX_POWER_TOO_HIGH = 15,  // DEPRECATED
  // (Deprecated) Password is used because Easy sign-in failed.
  // PASSWORD_ENTRY_LOGIN_FAILED = 16,
  // (Deprecated) Password is used because pairing data is changed for a "new" Chromebook
  // (where there was no previous pairing data).
  // PASSWORD_ENTRY_PAIRING_ADDED = 17,
  // Password is used because there is no smartlock state handler. Most likely
  // because Smart Lock is disabled, e.g. Bluetooth adapter not ready.
  PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER = 18,
  // Password is used because the phone is (a) locked, and (b) not right next to
  // the Chromebook.
  PASSWORD_ENTRY_PHONE_LOCKED_AND_RSSI_TOO_LOW = 19,

  // (Deprecated) Password entry was forced due to the reauth policy (e.g. the user must type
  // their password every 20 hours).
  // PASSWORD_ENTRY_FORCED_REAUTH = 20,

  // (Deprecated) Password entry was forced because sign-in with Smart Lock is disabled.
  // PASSWORD_ENTRY_LOGIN_DISABLED = 21,

  // Password is used because primary user was in background or user is
  // secondary user.
  PASSWORD_ENTRY_PRIMARY_USER_ABSENT = 22,

  SMART_LOCK_AUTH_EVENT_COUNT  // Must be the last entry.
};

void RecordSmartLockDidUserManuallyUnlockPhone(bool did_unlock);
void RecordSmartLockSigninDuration(const base::TimeDelta& duration);
void RecordSmartLockSigninEvent(SmartLockAuthEvent event);
void RecordSmartLockScreenUnlockDuration(const base::TimeDelta& duration);
void RecordSmartLockScreenUnlockEvent(SmartLockAuthEvent event);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_METRICS_H_
