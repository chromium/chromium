// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-lock-screen' allows the user to change how they unlock their
 * device.
 *
 * Example:
 *
 * <settings-lock-screen
 *   prefs="{{prefs}}">
 * </settings-lock-screen>
 */

Polymer({
  is: 'settings-lock-screen',

  behaviors: [
    I18nBehavior,
    LockStateBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {type: Object},

    /**
     * setModes_ is a partially applied function that stores the current auth
     * token. It's defined only when the user has entered a valid password.
     * @type {Object|undefined}
     * @private
     */
    setModes_: {
      type: Object,
      observer: 'onSetModesChanged_',
    },

    /**
     * Authentication token provided by lock-screen-password-prompt-dialog.
     */
    authToken: {
      type: String,
      value: '',
      notify: true,
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
      value: function() {
        return settings.recordLockScreenProgress;
      },
    },

    /**
     * True if quick unlock settings should be displayed on this machine.
     * @private
     */
    quickUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('quickUnlockEnabled');
      },
      readOnly: true,
    },

    /**
     * True if quick unlock settings are disabled by policy.
     * @private
     */
    quickUnlockDisabledByPolicy_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('quickUnlockDisabledByPolicy');
      },
      readOnly: true,
    },

    /**
     * True if fingerprint unlock settings should be displayed on this machine.
     * @private
     */
    fingerprintUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('fingerprintUnlockEnabled');
      },
      readOnly: true,
    },

    /** @private */
    numFingerprints_: {
      type: Number,
      value: 0,
    },

    /**
     * Whether notifications on the lock screen are enable by the feature flag.
     * @private
     */
    lockScreenNotificationsEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('lockScreenNotificationsEnabled');
      },
      readOnly: true,
    },

    /**
     * Whether the "hide sensitive notification" option on the lock screen can
     * be enable by the feature flag.
     * @private
     */
    lockScreenHideSensitiveNotificationSupported_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean(
            'lockScreenHideSensitiveNotificationsSupported');
      },
      readOnly: true,
    },

    /** @private */
    showPasswordPromptDialog_: Boolean,

    /** @private */
    showSetupPinDialog_: Boolean,
  },

  /** @private {?settings.FingerprintBrowserProxy} */
  fingerprintBrowserProxy_: null,

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /** @override */
  attached: function() {
    if (this.shouldAskForPassword_(settings.getCurrentRoute())) {
      this.openPasswordPromptDialog_();
    }

    this.fingerprintBrowserProxy_ =
        settings.FingerprintBrowserProxyImpl.getInstance();
    this.updateNumFingerprints_();
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    if (newRoute == settings.routes.LOCK_SCREEN) {
      this.updateUnlockType();
      this.updateNumFingerprints_();
    }

    if (this.shouldAskForPassword_(newRoute)) {
      this.openPasswordPromptDialog_();
    } else if (
        newRoute != settings.routes.FINGERPRINT &&
        oldRoute != settings.routes.FINGERPRINT) {
      // If the user navigated away from the lock screen settings page they will
      // have to re-enter their password. An exception is if they are navigating
      // to or from the fingerprint subpage.
      this.setModes_ = undefined;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onScreenLockChange_: function(event) {
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!this.authToken) {
      console.error('Screen lock changed with expired token.');
      target.checked = !target.checked;
      return;
    }
    this.setLockScreenEnabled(this.authToken, target.checked);
  },

  /**
   * Called when the unlock type has changed.
   * @param {!string} selected The current unlock type.
   * @private
   */
  selectedUnlockTypeChanged_: function(selected) {
    if (selected == LockScreenUnlockType.VALUE_PENDING) {
      return;
    }

    if (selected != LockScreenUnlockType.PIN_PASSWORD && this.setModes_) {
      this.setModes_.call(null, [], [], function(result) {
        assert(result, 'Failed to clear quick unlock modes');
        if (!result) {
          console.error('Failed to clear quick unlock modes');
        }
      });
    }
  },

  /** @private */
  onSetModesChanged_: function() {
    if (this.shouldAskForPassword_(settings.getCurrentRoute())) {
      this.showSetupPinDialog_ = false;
      this.openPasswordPromptDialog_();
    }
  },

  /** @private */
  openPasswordPromptDialog_: function() {
    this.showPasswordPromptDialog_ = true;
  },

  /** @private */
  onPasswordPromptDialogClose_: function() {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_) {
      settings.navigateToPreviousRoute();
    } else if (!this.$$('#unlockType').disabled) {
      cr.ui.focusWithoutInk(assert(this.$$('#unlockType')));
    } else {
      cr.ui.focusWithoutInk(assert(this.$$('#enableLockScreen')));
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigurePin_: function(e) {
    e.preventDefault();
    this.writeUma_(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  },

  /** @private */
  onSetupPinDialogClose_: function() {
    this.showSetupPinDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#setupPinButton')));
  },

  /**
   * Returns true if the setup pin section should be shown.
   * @param {!string} selectedUnlockType The current unlock type. Used to let
   *     Polymer know about the dependency.
   * @private
   */
  showConfigurePinButton_: function(selectedUnlockType) {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  },

  /**
   * @param {boolean} hasPin
   * @private
   */
  getSetupPinText_: function(hasPin) {
    if (hasPin) {
      return this.i18n('lockScreenChangePinButton');
    }
    return this.i18n('lockScreenSetupPinButton');
  },

  /** @private */
  getDescriptionText_: function() {
    if (this.numFingerprints_ > 0) {
      return this.i18n(
          'lockScreenNumberFingerprints', this.numFingerprints_.toString());
    }

    return this.i18n('lockScreenEditFingerprintsDescription');
  },

  /** @private */
  onEditFingerprints_: function() {
    settings.navigateTo(settings.routes.FINGERPRINT);
  },

  /**
   * @param {!settings.Route} route
   * @return {boolean} Whether the password dialog should be shown.
   * @private
   */
  shouldAskForPassword_: function(route) {
    return route == settings.routes.LOCK_SCREEN && !this.setModes_;
  },

  /** @private */
  updateNumFingerprints_: function() {
    if (this.fingerprintUnlockEnabled_ && this.fingerprintBrowserProxy_) {
      this.fingerprintBrowserProxy_.getNumFingerprints().then(
          numFingerprints => {
            this.numFingerprints_ = numFingerprints;
          });
    }
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @private
   */
  selectLockScreenOptionsString(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('lockScreenOptionsLoginLock');
    }
    return this.i18n('lockScreenOptionsLock');
  },
});
