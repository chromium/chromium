// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns true if a mandatory admin policy turns off
 * `credentials_enable_service`, which blocks saving new passwords.
 */
export function isSavingNewPasswordsBlockedByMandatoryPolicy(
    pref: chrome.settingsPrivate.PrefObject<boolean>): boolean {
  // Recommended policy only sets a default that the user can change.
  if (pref.enforcement !== chrome.settingsPrivate.Enforcement.ENFORCED) {
    return false;
  }
  // Extensions can set this pref through
  // chrome.privacy.services.passwordSavingEnabled. It's reported as ENFORCED,
  // but it isn't admin policy.
  if (pref.controlledBy === chrome.settingsPrivate.ControlledBy.EXTENSION) {
    return false;
  }
  return !pref.value;
}
