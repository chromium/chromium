// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/shared_vars_css.m.js';
import '../os_people_page/lock_screen_password_prompt_dialog.js';
import '../os_people_page/setup_pin_dialog.js';
import '../os_people_page/pin_autosubmit_dialog.js';

import {LockScreenProgress, recordLockScreenProgress} from '//resources/cr_components/chromeos/quick_unlock/lock_screen_constants.m.js';
import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenUnlockType, LockStateBehavior} from '../os_people_page/lock_state_behavior.m.js';

/**
 * @fileoverview
 * Subpage of settings-multidevice-notification-access-setup-dialog for setting
 * up screen lock.
 */
Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-multidevice-screen-lock-subpage',

  behaviors: [
    I18nBehavior,
    LockStateBehavior,
  ],

  properties: {

    /**
     * setModes_ is a partially applied function of
     * {@link chrome.quickUnlockPrivate.setModes} that stores the current auth
     * token. It's defined only when the user has entered a valid password.
     * @type {Object|undefined}
     * @private
     */
    setModes_: {
      type: Object,
    },

    /**
     * Authentication token.
     * @private {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken_: {
      type: Object,
      observer: 'onAuthTokenChanged_',
    },

    /**
     * writeUma_ is a function that handles writing uma stats. It may be
     * overridden for tests.
     *
     * @type {Function}
     * @private
     */
    writeUma_: {
      type: Object,
      value() {
        return recordLockScreenProgress;
      },
    },

    /**
     * True if quick unlock settings are disabled by policy.
     * @private
     */
    quickUnlockDisabledByPolicy_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('quickUnlockDisabledByPolicy');
      },
      readOnly: true,
    },

    /** Reflects whether the screen lock is enabled. */
    isScreenLockEnabled: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /** Reflects the password sub-dialog property. */
    isPasswordDialogShowing: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /** @private {boolean} */
    shouldPromptPasswordDialog_: Boolean,

    /** @private */
    showSetupPinDialog_: Boolean,

    /** @private */
    showPinAutosubmitDialog_: Boolean,
  },

  /** @override */
  created() {
    if (this.authToken_ === undefined) {
      this.shouldPromptPasswordDialog_ = true;
    }
  },

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /**
   * Called when the unlock type has changed.
   * @param {!string} selected The current unlock type.
   * @private
   */
  selectedUnlockTypeChanged_(selected) {
    if (selected === LockScreenUnlockType.PASSWORD && this.setModes_) {
      // If the user selects PASSWORD only (which sends an asynchronous
      // setModes_.call() to clear the quick unlock capability), indicate to the
      // user immediately that the quick unlock capability is cleared by setting
      // |hasPin| to false. If there is an error clearing quick unlock, revert
      // |hasPin| to true. This prevents setupPinButton UI delays, except in the
      // small chance that CrOS fails to remove the quick unlock capability. See
      // https://crbug.com/1054327 for details.
      this.hasPin = false;
      this.setModes_.call(null, [], [], (result) => {
        // Revert |hasPin| to true in the event setModes_ fails to set lock
        // state to PASSWORD only.
        if (!result) {
          this.hasPin = true;
        }

        assert(result, 'Failed to clear quick unlock modes');
      });
    }
  },

  /** @private */
  onAuthTokenChanged_() {
    if (this.authToken_ === undefined) {
      this.setModes_ = undefined;
    } else {
      this.setModes_ = (modes, credentials, onComplete) => {
        this.quickUnlockPrivate.setModes(
            this.authToken_.token, modes, credentials, () => {
              let result = true;
              if (chrome.runtime.lastError) {
                console.error(
                    'setModes failed: ' + chrome.runtime.lastError.message);
                result = false;
              }
              onComplete(result);
            });
      };
    }
  },

  /** @private */
  onPasswordPromptDialogClose_() {
    this.shouldPromptPasswordDialog_ = false;
  },

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   * */
  onAuthTokenObtained_(e) {
    this.authToken_ = e.detail;
    this.setLockScreenEnabled(this.authToken_.token, true);
    this.isScreenLockEnabled = true;
    // Avoid dialog.close() of password_prompt_dialog.ts to close main dialog
    this.isPasswordDialogShowing = true;
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @return {string}
   * @private
   */
  selectLockScreenOptionsString(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('lockScreenOptionsLoginLock');
    }
    return this.i18n('lockScreenOptionsLock');
  },

  /**
   * Returns true if the setup pin section should be shown.
   * @param {!string} selectedUnlockType The current unlock type. Used to let
   *     Polymer know about the dependency.
   * @private
   */
  showConfigurePinButton_(selectedUnlockType) {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigurePin_(e) {
    e.preventDefault();
    this.writeUma_(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  },

  /** @private */
  onSetupPinDialogClose_() {
    this.showSetupPinDialog_ = false;
    focusWithoutInk(assert(this.$$('#setupPinButton')));
  },

  /**
   * @param {boolean} hasPin
   * @return {string}
   * @private
   */
  getSetupPinText_(hasPin) {
    if (hasPin) {
      return this.i18n('lockScreenChangePinButton');
    }
    return this.i18n('lockScreenSetupPinButton');
  },

});
