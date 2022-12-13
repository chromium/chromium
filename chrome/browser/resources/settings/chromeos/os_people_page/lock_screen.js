// Copyright 2016 The Chromium Authors
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

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '../../controls/settings_toggle_button.js';
import './setup_pin_dialog.js';
import './pin_autosubmit_dialog.js';
import './local_data_recovery_dialog.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';
import '../multidevice_page/multidevice_smartlock_item.js';

import {focusWithoutInk} from 'chrome://resources/ash/common/focus_without_ink_js.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {LockScreenProgress, recordLockScreenProgress} from 'chrome://resources/ash/common/quick_unlock/lock_screen_constants.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {AuthFactor, FactorObserverInterface, FactorObserverReceiver, ManagementType, RecoveryFactorEditor_ConfigureResult} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {FingerprintBrowserProxy, FingerprintBrowserProxyImpl} from './fingerprint_browser_proxy.js';
import {LockScreenUnlockType, LockStateBehavior, LockStateBehaviorInterface} from './lock_state_behavior.js';
import {getPluralStringFromProxy} from './plural_string_proxy_wrapper.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {FactorObserverInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {LockStateBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsLockScreenElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      LockStateBehavior,
      WebUIListenerBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsLockScreenElement extends SettingsLockScreenElementBase {
  static get is() {
    return 'settings-lock-screen';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * True if fingerprint unlock settings should be displayed on this
       * machine.
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
       * Whether notifications on the lock screen are enable by the feature
       * flag.
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
       * State of the recovery toggle. Is |null| iff recovery is not a
       * available.
       * @type {?chrome.settingsPrivate.PrefObject}
       * @private
       */
      recovery_: {
        type: Object,
        value: null,
      },

      /** @private*/
      recoveryChangeInProcess_: {
        type: Boolean,
        value: false,
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

      /** @private */
      showDisableRecoveryDialog_: Boolean,

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kLockScreenV2,
          Setting.kChangeAuthPinV2,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'selectedUnlockTypeChanged_(selectedUnlockType)',
      'updateRecoveryState_(authToken)',
    ];
  }

  constructor() {
    super();

    /** @private {!FingerprintBrowserProxy} */
    this.fingerprintBrowserProxy_ = FingerprintBrowserProxyImpl.getInstance();

    /** @private {string} */
    this.numFingerprintDescription_ = '';
  }

  /** @override */
  ready() {
    super.ready();
    // Register observer for auth factor updates.
    // TODO(crbug/1321440): Are we leaking |this| here because we never remove
    // the observer? We could close the pipe with |$.close()|, but not clear
    // whether that removes all references to |receiver| and then eventually to
    // |this|.
    const receiver = new FactorObserverReceiver(this);
    const remote = receiver.$.bindNewPipeAndPassRemote();
    this.authFactorConfig.observeFactorChanges(remote);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.updateNumFingerprints_();

    this.addWebUIListener(
        'quick-unlock-disabled-by-policy-changed',
        (quickUnlockDisabledByPolicy) => {
          this.quickUnlockDisabledByPolicy_ = quickUnlockDisabledByPolicy;
        });
    chrome.send('RequestQuickUnlockDisabledByPolicy');
  }

  /**
   * Overridden from RouteObserverBehavior.
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
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
  }

  // Dispatch an event that signal that the auth token is invalid. This will
  // reopen the password prompt.
  dispatchAuthTokenInvalidEvent_() {
    const authTokenInvalid =
        new CustomEvent('auth-token-invalid', {bubbles: true, composed: true});
    this.dispatchEvent(authTokenInvalid);
  }

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
    this.setLockScreenEnabled(
        this.authToken.token, target.checked, (success) => {
          if (!success) {
            target.checked = !target.checked;
            this.dispatchAuthTokenInvalidEvent_();
          }
        });
  }

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
  }

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
  }

  /** @private */
  focusDefaultElement_() {
    afterNextRender(this, () => {
      if (!this.shadowRoot.querySelector('#unlockType').disabled) {
        focusWithoutInk(assert(this.shadowRoot.querySelector('#unlockType')));
      } else {
        focusWithoutInk(
            assert(this.shadowRoot.querySelector('#enableLockScreen')));
      }
    });
  }

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
  }

  /**
   * @param {!Event} e
   * @private
   */
  onConfigurePin_(e) {
    e.preventDefault();
    this.writeUma_(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  }

  /** @private */
  onSetupPinDialogClose_() {
    this.showSetupPinDialog_ = false;
    focusWithoutInk(assert(this.shadowRoot.querySelector('#setupPinButton')));
  }

  /** @private */
  onPinAutosubmitDialogClose_() {
    this.showPinAutosubmitDialog_ = false;
    focusWithoutInk(
        assert(this.shadowRoot.querySelector('#enablePinAutoSubmit')));
  }

  /** @private */
  onRecoveryDialogClose_() {
    this.showDisableRecoveryDialog_ = false;
    this.recoveryChangeInProcess_ = false;
    focusWithoutInk(assert(this.shadowRoot.querySelector('#recoveryToggle')));
  }

  /**
   * Returns true if the setup pin section should be shown.
   * @param {!string} selectedUnlockType The current unlock type. Used to let
   *     Polymer know about the dependency.
   * @private
   */
  showConfigurePinButton_(selectedUnlockType) {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  }

  /**
   * @param {boolean} hasPin
   * @private
   */
  getSetupPinText_(hasPin) {
    if (hasPin) {
      return this.i18n('lockScreenChangePinButton');
    }
    return this.i18n('lockScreenSetupPinButton');
  }

  /** @private */
  updateNumFingerprintsDescription_() {
    if (this.numFingerprints_ === 0) {
      this.numFingerprintDescription_ =
          this.i18n('lockScreenEditFingerprintsDescription');
    } else {
      getPluralStringFromProxy(
          'lockScreenNumberFingerprints', this.numFingerprints_)
          .then(string => this.numFingerprintDescription_ = string);
    }
  }

  /** @private */
  onEditFingerprints_() {
    Router.getInstance().navigateTo(routes.FINGERPRINT);
  }

  /**
   * @return {boolean} Whether an event was fired to show the password dialog.
   * @private
   */
  requestPasswordIfApplicable_() {
    const currentRoute = Router.getInstance().getCurrentRoute();
    if (currentRoute === routes.LOCK_SCREEN && !this.setModes) {
      const event = new CustomEvent(
          'password-requested', {bubbles: true, composed: true});
      this.dispatchEvent(event);
      return true;
    }
    return false;
  }

  /** @private */
  updateNumFingerprints_() {
    if (this.fingerprintUnlockEnabled_ && this.fingerprintBrowserProxy_) {
      this.fingerprintBrowserProxy_.getNumFingerprints().then(
          numFingerprints => {
            this.numFingerprints_ = numFingerprints;
          });
    }
  }

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
  }

  /**
   * Called by chrome when the state of an auth factor changes.
   * @param {!AuthFactor} factor
   * @private
   * */
  onFactorChanged(factor) {
    switch (factor) {
      case AuthFactor.kRecovery:
        this.updateRecoveryState_(this.authToken);
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * Fetches state of an auth factor from the backend. Returns a |PrefObject|
   * suitable for use with a boolean toggle, or |null| if the auth factor is
   * not available.
   * @private
   * @param {!AuthFactor} authFactor
   * @return {!Promise<?chrome.settingsPrivate.PrefObject>}
   */
  async fetchFactorState_(authFactor) {
    const token = this.authToken.token;

    const {supported} =
        await this.authFactorConfig.isSupported(token, authFactor);
    if (!supported) {
      return null;
    }

    // Fetch properties of the factor concurrently.
    const [{configured}, {management}, {editable}] = await Promise.all([
      this.authFactorConfig.isConfigured(token, authFactor),
      this.authFactorConfig.getManagementType(token, authFactor),
      this.authFactorConfig.isEditable(token, authFactor),
    ]);

    const state = {
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: configured,
      key: '',
    };

    if (management !== ManagementType.kNone) {
      if (management === ManagementType.kDevice) {
        state.managed = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
      } else {
        assert(management === ManagementType.kUser, 'Invalid management type');
        state.managed = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      }

      if (editable) {
        state.enforcement = chrome.settingsPrivate.Enforcement.RECOMMENDED;
      } else {
        state.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      }
    }

    return state;
  }

  /**
   * Fetches the state of the recovery factor and updates the corresponding
   * property.
   * @private
   * @param {!chrome.quickUnlockPrivate.TokenInfo|undefined} authToken
   */
  async updateRecoveryState_(authToken) {
    if (!authToken) {
      return;
    }
    assert(authToken.token === this.authToken.token);
    this.recovery_ = await this.fetchFactorState_(AuthFactor.kRecovery);
  }

  /**
   * Called when the user flips the recovery toggle.
   * @private
   */
  async onRecoveryChange_(event) {
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    // Reset checkbox to its previous state and disable it. If we succeed to
    // enable/disable recovery, this is updated automatically because the
    // pref value changes.
    const shouldEnable = target.checked;
    target.resetToPrefValue();
    if (this.recoveryChangeInProcess_) {
      return;
    }
    this.recoveryChangeInProcess_ = true;
    if (!shouldEnable) {
      this.showDisableRecoveryDialog_ = true;
      return;
    }
    try {
      if (!this.authToken) {
        this.dispatchAuthTokenInvalidEvent_();
        return;
      }

      const {result} = await this.recoveryFactorEditor.configure(
          this.authToken.token, shouldEnable);
      switch (result) {
        case RecoveryFactorEditor_ConfigureResult.kSuccess:
          break;
        case RecoveryFactorEditor_ConfigureResult.kInvalidTokenError:
          // This will open the password prompt.
          this.dispatchAuthTokenInvalidEvent_();
          return;
        case RecoveryFactorEditor_ConfigureResult.kClientError:
          console.error('Error configuring recovery');
          return;
      }
    } finally {
      this.recoveryChangeInProcess_ = false;
    }
  }
}

customElements.define(SettingsLockScreenElement.is, SettingsLockScreenElement);
