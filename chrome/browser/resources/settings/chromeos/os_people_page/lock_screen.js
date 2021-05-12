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
    DeepLinkingBehavior,
    I18nBehavior,
    LockStateBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {type: Object},

    /**
     * setModes is a partially applied function that stores the current auth
     * token. It's defined only when the user has entered a valid password.
     * @type {Object|undefined}
     */
    setModes: {
      type: Object,
      observer: 'onSetModesChanged_',
    },

    /**
     * Authentication token provided by lock-screen-password-prompt-dialog.
     * @type {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken: {
      type: Object,
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
      value() {
        return settings.recordLockScreenProgress;
      },
    },

    /**
     * True if quick unlock settings should be displayed on this machine.
     * @private
     */
    quickUnlockEnabled_: {
      type: Boolean,
      value() {
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
      value() {
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
      value() {
        return loadTimeData.getBoolean('fingerprintUnlockEnabled');
      },
      readOnly: true,
    },

    /** @private */
    numFingerprints_: {
      type: Number,
      value: 0,
      observer: 'updateNumFingerprintsDescription_',
    },

    /** @private */
    numFingerprintsDescription_: {
      type: String,
    },

    /**
     * Whether notifications on the lock screen are enable by the feature flag.
     * @private
     */
    lockScreenNotificationsEnabled_: {
      type: Boolean,
      value() {
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
      value() {
        return loadTimeData.getBoolean(
            'lockScreenHideSensitiveNotificationsSupported');
      },
      readOnly: true,
    },

    /**
     * True if quick unlock settings should be displayed on this machine.
     * @private
     */
    quickUnlockPinAutosubmitFeatureEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean(
            'quickUnlockPinAutosubmitFeatureEnabled');
      },
      readOnly: true,
    },

    /** @private */
    showSetupPinDialog_: Boolean,

    /** @private */
    showPinAutosubmitDialog_: Boolean,

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kLockScreen,
        chromeos.settings.mojom.Setting.kChangeAuthPin,
        chromeos.settings.mojom.Setting.kLockScreenV2,
        chromeos.settings.mojom.Setting.kChangeAuthPinV2,
      ]),
    },
  },

  /** @private {?settings.FingerprintBrowserProxy} */
  fingerprintBrowserProxy_: null,

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /** @override */
  attached() {
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
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute === settings.routes.LOCK_SCREEN) {
      this.updateUnlockType(/*activeModesChanged=*/ false);
      this.updateNumFingerprints_();
      this.attemptDeepLink();
    }

    if (this.requestPasswordIfApplicable_()) {
      this.showSetupPinDialog_ = false;
      this.showPinAutosubmitDialog_ = false;
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onScreenLockChange_(event) {
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!this.authToken) {
      console.error('Screen lock changed with expired token.');
      target.checked = !target.checked;
      return;
    }
    this.setLockScreenEnabled(this.authToken.token, target.checked);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onPinAutosubmitChange_(event) {
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!this.authToken) {
      console.error('PIN autosubmit setting changed with expired token.');
      target.checked = !target.checked;
      return;
    }

    // Read-only preference. Changes will be reflected directly on the toggle.
    const autosubmitEnabled = target.checked;
    target.resetToPrefValue();

    if (autosubmitEnabled) {
      this.showPinAutosubmitDialog_ = true;
    } else {
      // Call quick unlock to disable the auto-submit option.
      this.quickUnlockPrivate.setPinAutosubmitEnabled(
          this.authToken.token, '' /* PIN */, false /*enabled*/, function() {});
    }
  },

  /**
   * Called when the unlock type has changed.
   * @param {!string} selected The current unlock type.
   * @private
   */
  selectedUnlockTypeChanged_(selected) {
    if (selected === LockScreenUnlockType.VALUE_PENDING) {
      return;
    }

    if (selected !== LockScreenUnlockType.PIN_PASSWORD && this.setModes) {
      // If the user selects PASSWORD only (which sends an asynchronous
      // setModes.call() to clear the quick unlock capability), indicate to the
      // user immediately that the quick unlock capability is cleared by setting
      // |hasPin| to false. If there is an error clearing quick unlock, revert
      // |hasPin| to true. This prevents setupPinButton UI delays, except in the
      // small chance that CrOS fails to remove the quick unlock capability. See
      // https://crbug.com/1054327 for details.
      if (!this.hasPin) {
        return;
      }
      this.hasPin = false;
      this.setModes.call(null, [], [], (result) => {
        assert(result, 'Failed to clear quick unlock modes');
        // Revert |hasPin| to true in the event setModes fails to set lock state
        // to PASSWORD only.
        this.hasPin = true;
      });
    }
  },

  /** @private */
  focusDefaultElement_() {
    Polymer.RenderStatus.afterNextRender(this, () => {
      if (!this.$$('#unlockType').disabled) {
        cr.ui.focusWithoutInk(assert(this.$$('#unlockType')));
      } else {
        cr.ui.focusWithoutInk(assert(this.$$('#enableLockScreen')));
      }
    });
  },

  /** @private */
  onSetModesChanged_() {
    if (this.requestPasswordIfApplicable_()) {
      this.showSetupPinDialog_ = false;
      this.showPinAutosubmitDialog_ = false;
      return;
    }

    if (settings.Router.getInstance().getCurrentRoute() ===
        settings.routes.LOCK_SCREEN) {
      // Show deep links again if the user authentication dialog just closed.
      this.attemptDeepLink().then(result => {
        // If there were no supported deep links, focus the default element.
        if (result.pendingSettingId == null) {
          this.focusDefaultElement_();
        }
      });
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigurePin_(e) {
    e.preventDefault();
    this.writeUma_(settings.LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  },

  /** @private */
  onSetupPinDialogClose_() {
    this.showSetupPinDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#setupPinButton')));
  },

  /** @private */
  onPinAutosubmitDialogClose_() {
    this.showPinAutosubmitDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#enablePinAutoSubmit')));
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
   * @param {boolean} hasPin
   * @private
   */
  getSetupPinText_(hasPin) {
    if (hasPin) {
      return this.i18n('lockScreenChangePinButton');
    }
    return this.i18n('lockScreenSetupPinButton');
  },

  /** @private */
  updateNumFingerprintsDescription_() {
    if (this.numFingerprints_ === 0) {
      this.numFingerprintDescription_ =
          this.i18n('lockScreenEditFingerprintsDescription');
    } else {
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'lockScreenNumberFingerprints', this.numFingerprints_)
          .then(string => this.numFingerprintDescription_ = string);
    }
  },

  /** @private */
  onEditFingerprints_() {
    settings.Router.getInstance().navigateTo(settings.routes.FINGERPRINT);
  },

  /**
   * @return {boolean} Whether an event was fired to show the password dialog.
   * @private
   */
  requestPasswordIfApplicable_() {
    const currentRoute = settings.Router.getInstance().getCurrentRoute();
    if (currentRoute === settings.routes.LOCK_SCREEN && !this.setModes) {
      this.fire('password-requested');
      return true;
    }
    return false;
  },

  /** @private */
  updateNumFingerprints_() {
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
