// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
// #import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';

/**
 * @fileoverview
 * Contains utilities that help identify the current way that the lock screen
 * will be displayed.
 */

/** @enum {string} */
/* #export */ const LockScreenUnlockType = {
  VALUE_PENDING: 'value_pending',
  PASSWORD: 'password',
  PIN_PASSWORD: 'pin+password'
};

/**
 * Determining if the device supports PIN sign-in takes time, as it may require
 * a cryptohome call. This means incorrect strings may be shown for a brief
 * period, and updating them causes UI flicker.
 *
 * Cache the value since the behavior is instantiated multiple times. Caching
 * is safe because PIN login support depends only on hardware capabilities. The
 * value does not change after discovered.
 *
 * @type {boolean|undefined}
 */
let cachedHasPinLogin = undefined;

/** @polymerBehavior */
/* #export */ const LockStateBehaviorImpl = {
  properties: {
    /**
     * The currently selected unlock type.
     * @type {!LockScreenUnlockType}
     */
    selectedUnlockType:
        {type: String, notify: true, value: LockScreenUnlockType.VALUE_PENDING},

    /**
     * True/false if there is a PIN set; undefined if the computation is still
     * pending. This is a separate value from selectedUnlockType because the UI
     * can change the selectedUnlockType before setting up a PIN.
     * @type {boolean|undefined}
     */
    hasPin: {type: Boolean, notify: true},

    /**
     * True if the PIN backend supports signin. undefined iff the value is still
     * resolving.
     * @type {boolean|undefined}
     */
    hasPinLogin: {type: Boolean, notify: true},

    /**
     * Interface for chrome.quickUnlockPrivate calls. May be overridden by
     * tests.
     * @type {QuickUnlockPrivate}
     */
    quickUnlockPrivate: {type: Object, value: chrome.quickUnlockPrivate},
  },

  /** @override */
  attached() {
    this.boundOnActiveModesChanged_ =
        this.updateUnlockType.bind(this, /*activeModesChanged=*/ true);
    this.quickUnlockPrivate.onActiveModesChanged.addListener(
        this.boundOnActiveModesChanged_);

    // See comment on |cachedHasPinLogin| declaration.
    if (cachedHasPinLogin === undefined) {
      this.addWebUIListener(
          'pin-login-available-changed',
          this.handlePinLoginAvailableChanged_.bind(this));
      chrome.send('RequestPinLoginState');
    } else {
      this.hasPinLogin = cachedHasPinLogin;
    }

    this.updateUnlockType(/*activeModesChanged=*/ false);
  },

  /** @override */
  detached() {
    this.quickUnlockPrivate.onActiveModesChanged.removeListener(
        this.boundOnActiveModesChanged_);
  },

  /**
   * Updates the selected unlock type radio group. This function will get called
   * after preferences are initialized, after the quick unlock mode has been
   * changed, and after the lockscreen preference has changed.
   *
   * @param {boolean} activeModesChanged If the function is called because
   *     active modes have changed.
   */
  updateUnlockType(activeModesChanged) {
    this.quickUnlockPrivate.getActiveModes(modes => {
      if (modes.includes(chrome.quickUnlockPrivate.QuickUnlockMode.PIN)) {
        this.hasPin = true;
        this.selectedUnlockType = LockScreenUnlockType.PIN_PASSWORD;
      } else {
        // A race condition can occur:
        // (1) User selects PIN_PASSSWORD, and successfully sets a pin, adding
        //     QuickUnlockMode.PIN to active modes.
        // (2) User selects PASSWORD, QuickUnlockMode.PIN capability is cleared
        //     from the active modes, notifying LockStateBehavior to call
        //     updateUnlockType to fetch the active modes asynchronously.
        // (3) User selects PIN_PASSWORD, but the process from step 2 has
        //     not yet completed.
        // In this case, do not forcibly select the PASSWORD radio button even
        // though the unlock type is still PASSWORD (|hasPin| is false). If the
        // user wishes to set a pin, they will have to click the set pin button.
        // See https://crbug.com/1054327 for details.
        if (activeModesChanged && !this.hasPin &&
            this.selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD) {
          return;
        }
        this.hasPin = false;
        this.selectedUnlockType = LockScreenUnlockType.PASSWORD;
      }
    });
  },

  /**
   * Sets the lock screen enabled state.
   * @param {string} authToken The token returned by
   *                           QuickUnlockPrivate.getAuthToken
   * @param {boolean} enabled
   * @see quickUnlockPrivate.setLockScreenEnabled
   */
  setLockScreenEnabled(authToken, enabled) {
    this.quickUnlockPrivate.setLockScreenEnabled(authToken, enabled);
  },

  /**
   * Handler for when the pin login available state has been updated.
   * @private
   */
  handlePinLoginAvailableChanged_(isAvailable) {
    this.hasPinLogin = isAvailable;
    cachedHasPinLogin = this.hasPinLogin;
  },
};

/** @polymerBehavior */
/* #export */ const LockStateBehavior =
    [I18nBehavior, WebUIListenerBehavior, LockStateBehaviorImpl];
