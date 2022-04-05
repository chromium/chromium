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

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '../../controls/settings_toggle_button.js';
import './setup_pin_dialog.js';
import './pin_autosubmit_dialog.js';
import '../../prefs/prefs.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';
import '../multidevice_page/multidevice_smartlock_item.js';

import {LockScreenProgress, recordLockScreenProgress} from '//resources/cr_components/chromeos/quick_unlock/lock_screen_constants.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {FingerprintAttempt, FingerprintBrowserProxy, FingerprintBrowserProxyImpl, FingerprintInfo, FingerprintResultType, FingerprintScan} from './fingerprint_browser_proxy.js';
import {LockScreenUnlockType, LockStateBehavior, LockStateBehaviorImpl} from './lock_state_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-lock-screen',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    LockStateBehavior,
    WebUIListenerBehavior,
    RouteObserverBehavior,
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
        return recordLockScreenProgress;
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
      value: loadTimeData.getBoolean('quickUnlockDisabledByPolicy'),
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

    /**
     * Alias for the SmartLockUIRevamp feature flag.
     * @private
     */
    smartLockUIRevampEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('smartLockUIRevampEnabled');
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
        chromeos.settings.mojom.Setting.kLockScreenV2,
        chromeos.settings.mojom.Setting.kChangeAuthPinV2,
      ]),
    },
  },

  /** @private {?FingerprintBrowserProxy} */
  fingerprintBrowserProxy_: null,

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /** @override */
  attached() {
    this.fingerprintBrowserProxy_ = FingerprintBrowserProxyImpl.getInstance();
    this.updateNumFingerprints_();

    this.addWebUIListener(
        'quick-unlock-disabled-by-policy-changed',
        (quickUnlockDisabledByPolicy) => {
          this.quickUnlockDisabledByPolicy_ = quickUnlockDisabledByPolicy;
        });
    chrome.send('RequestQuickUnlockDisabledByPolicy');
  },

  /**
   * Overridden from RouteObserverBehavior.
   * @param {!Route} newRoute
   * @param {!Route} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute === routes.LOCK_SCREEN) {
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
      this.hasPin = false;
      this.setModes.call(null, [], [], (result) => {
        // Revert |hasPin| to true in the event setModes fails to set lock state
        // to PASSWORD only.
        if (!result) {
          this.hasPin = true;
        }

        assert(result, 'Failed to clear quick unlock modes');
      });
    }
  },

  /** @private */
  focusDefaultElement_() {
    afterNextRender(this, () => {
      if (!this.$$('#unlockType').disabled) {
        focusWithoutInk(assert(this.$$('#unlockType')));
      } else {
        focusWithoutInk(assert(this.$$('#enableLockScreen')));
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

    if (Router.getInstance().getCurrentRoute() === routes.LOCK_SCREEN) {
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
    this.writeUma_(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  },

  /** @private */
  onSetupPinDialogClose_() {
    this.showSetupPinDialog_ = false;
    focusWithoutInk(assert(this.$$('#setupPinButton')));
  },

  /** @private */
  onPinAutosubmitDialogClose_() {
    this.showPinAutosubmitDialog_ = false;
    focusWithoutInk(assert(this.$$('#enablePinAutoSubmit')));
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
    Router.getInstance().navigateTo(routes.FINGERPRINT);
  },

  /**
   * @return {boolean} Whether an event was fired to show the password dialog.
   * @private
   */
  requestPasswordIfApplicable_() {
    const currentRoute = Router.getInstance().getCurrentRoute();
    if (currentRoute === routes.LOCK_SCREEN && !this.setModes) {
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
