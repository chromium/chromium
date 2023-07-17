// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-lock-screen-subpage' allows the user to change how they unlock
 * their device.
 *
 * Example:
 *
 * <settings-lock-screen-subpage
 *   prefs="{{prefs}}">
 * </settings-lock-screen-subpage>
 */

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '/shared/settings/controls/settings_toggle_button.js';
import './setup_pin_dialog.js';
import './pin_autosubmit_dialog.js';
import './local_data_recovery_dialog.js';
import '../settings_shared.css.js';
import '../multidevice_page/multidevice_smartlock_item.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {LockScreenProgress, recordLockScreenProgress} from 'chrome://resources/ash/common/quick_unlock/lock_screen_constants.js';
import {fireAuthTokenInvalidEvent} from 'chrome://resources/ash/common/quick_unlock/utils.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {AuthFactor, ConfigureResult, FactorObserverReceiver, ManagementType, PinFactorEditor} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {LockScreenUnlockType, LockStateMixin} from '../lock_state_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {FingerprintBrowserProxy, FingerprintBrowserProxyImpl} from './fingerprint_browser_proxy.js';
import {getTemplate} from './lock_screen_subpage.html.js';

const SettingsLockScreenElementBase =
    RouteObserverMixin(LockStateMixin(DeepLinkingMixin(PolymerElement)));

export class SettingsLockScreenElement extends SettingsLockScreenElementBase {
  static get is() {
    return 'settings-lock-screen-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {type: Object},

      /**
       * Authentication token provided by lock-screen-password-prompt-dialog.
       */
      authToken: {
        type: String,
        notify: true,
        observer: 'onAuthTokenChanged_',
      },

      /**
       * True if quick unlock settings should be displayed on this machine.
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
       */
      quickUnlockDisabledByPolicy_: {
        type: Boolean,
        value: loadTimeData.getBoolean('quickUnlockDisabledByPolicy'),
      },

      /**
       * True if fingerprint unlock settings should be displayed on this
       * machine.
       */
      fingerprintUnlockEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('fingerprintUnlockEnabled');
        },
        readOnly: true,
      },

      numFingerprints_: {
        type: Number,
        value: 0,
        observer: 'updateNumFingerprintsDescription_',
      },

      numFingerprintsDescription_: {
        type: String,
      },

      /**
       * Whether notifications on the lock screen are enable by the feature
       * flag.
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
       * True if cryptohome recovery feature is enabled.
       */
      cryptohomeRecoveryEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('cryptohomeRecoveryEnabled');
        },
        readOnly: true,
      },

      /**
       * State of the recovery toggle. Is |null| iff recovery is not a
       * available.
       */
      recovery_: {
        type: Object,
        value: null,
      },

      recoveryChangeInProcess_: {
        type: Boolean,
        value: false,
      },

      hasPin: {
        type: Boolean,
        value: false,
      },

      /**
       * True if quick unlock settings should be displayed on this machine.
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
       */
      smartLockUIRevampEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('smartLockUIRevampEnabled');
        },
        readOnly: true,
      },

      noRecoveryVirtualPref_: Object,

      showSetupPinDialog_: Boolean,

      showPinAutosubmitDialog_: Boolean,

      showDisableRecoveryDialog_: Boolean,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kLockScreenV2,
          Setting.kChangeAuthPinV2,
          Setting.kLockScreenNotification,
        ]),
      },
    };
  }

  prefs: Object;
  authToken: string|undefined;
  private quickUnlockEnabled_: boolean;
  private quickUnlockDisabledByPolicy_: boolean;
  private fingerprintUnlockEnabled_: boolean;
  private numFingerprints_: number;
  private numFingerprintDescription_: string;
  private lockScreenNotificationsEnabled_: boolean;
  private lockScreenHideSensitiveNotificationSupported_: boolean;
  private cryptohomeRecoveryEnabled_: boolean;
  private recovery_: chrome.settingsPrivate.PrefObject|null;
  private noRecoveryVirtualPref_: chrome.settingsPrivate.PrefObject;
  private recoveryChangeInProcess_: boolean;
  private hasPin: boolean;
  private quickUnlockPinAutosubmitFeatureEnabled_: boolean;
  private smartLockUIRevampEnabled_: boolean;
  private showSetupPinDialog_: boolean;
  private showPinAutosubmitDialog_: boolean;
  private showDisableRecoveryDialog_: boolean;
  private fingerprintBrowserProxy_: FingerprintBrowserProxy;

  static get observers() {
    return [
      'selectedUnlockTypeChanged_(selectedUnlockType)',
      'updateRecoveryState_(authToken)',
      'updatePinState_(authToken)',
    ];
  }

  constructor() {
    super();

    this.fingerprintBrowserProxy_ = FingerprintBrowserProxyImpl.getInstance();

    this.numFingerprintDescription_ = '';
    // The pref is used to bind to the settings toggle when the `recovery_` pref
    // is not set because the recovery feature is not available on the device.
    this.noRecoveryVirtualPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
  }

  override ready(): void {
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

  override connectedCallback() {
    super.connectedCallback();

    this.updateNumFingerprints_();

    this.addWebUiListener(
        'quick-unlock-disabled-by-policy-changed',
        (quickUnlockDisabledByPolicy: boolean) => {
          this.quickUnlockDisabledByPolicy_ = quickUnlockDisabledByPolicy;
        });
    chrome.send('RequestQuickUnlockDisabledByPolicy');
  }

  override currentRouteChanged(newRoute: Route) {
    if (newRoute === routes.LOCK_SCREEN) {
      this.updatePinState_(this.authToken);
      this.updateNumFingerprints_();
      this.attemptDeepLink();
    }

    if (this.requestPasswordIfApplicable_()) {
      this.showSetupPinDialog_ = false;
      this.showPinAutosubmitDialog_ = false;
    }
  }

  private onScreenLockChange_(event: Event): void {
    const target = event.target as SettingsToggleButtonElement;
    if (typeof this.authToken !== 'string') {
      console.error('Screen lock changed with expired token.');
      target.checked = !target.checked;
      return;
    }
    this.setLockScreenEnabled(this.authToken, target.checked, (success) => {
      if (!success) {
        target.checked = !target.checked;
        fireAuthTokenInvalidEvent(this);
      }
    });
  }

  private onPinAutosubmitChange_(event: Event): void {
    const target = event.target as SettingsToggleButtonElement;
    if (typeof this.authToken !== 'string') {
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
          this.authToken, '' /* PIN */, false /*enabled*/, () => {});
    }
  }

  private async selectedUnlockTypeChanged_(selected: string): Promise<void> {
    switch (selected) {
      case LockScreenUnlockType.VALUE_PENDING:
      case LockScreenUnlockType.PIN_PASSWORD:
        return;
      case LockScreenUnlockType.PASSWORD: {
        if (typeof this.authToken !== 'string') {
          // This is not an error scenario, since the selected unlock type
          // changes from the initial VALUE_PENDING to either PIN_PASSWORD or
          // PASSWORD initially.
          fireAuthTokenInvalidEvent(this);
          return;
        }
        // If the user selects PASSWORD only (which sends an asynchronous
        // PinFactorEditor.removePin() to clear the quick unlock capability),
        // indicate to the user immediately that the quick unlock capability is
        // cleared by setting |hasPin| to false. If there is an error clearing
        // quick unlock, revert |hasPin| to true. This prevents setupPinButton
        // UI delays, except in the small chance that CrOS fails to remove the
        // quick unlock capability. See https://crbug.com/1054327 for details.
        this.hasPin = false;
        const {result} =
            await PinFactorEditor.getRemote().removePin(this.authToken);
        if (result !== ConfigureResult.kSuccess) {
          this.hasPin = true;
        }

        switch (result) {
          case ConfigureResult.kSuccess:
            break;
          case ConfigureResult.kInvalidTokenError:
            fireAuthTokenInvalidEvent(this);
            break;
          case ConfigureResult.kFatalError:
            console.error('Error removing PIN');
            break;
        }
        return;
      }
      default:
        assertNotReached();
    }
  }

  private focusDefaultElement_(): void {
    afterNextRender(this, () => {
      if (!this.shadowRoot!.querySelector<CrRadioGroupElement>(
                               '#unlockType')!.disabled) {
        focusWithoutInk(
            castExists(this.shadowRoot!.querySelector('#unlockType')));
      } else {
        focusWithoutInk(
            castExists(this.shadowRoot!.querySelector('#enableLockScreen')));
      }
    });
  }

  private onAuthTokenChanged_(): void {
    if (this.requestPasswordIfApplicable_()) {
      this.showSetupPinDialog_ = false;
      this.showPinAutosubmitDialog_ = false;
      return;
    }

    if (Router.getInstance().currentRoute === routes.LOCK_SCREEN) {
      // Show deep links again if the user authentication dialog just closed.
      this.attemptDeepLink().then(result => {
        // If there were no supported deep links, focus the default element.
        if (result.pendingSettingId == null) {
          this.focusDefaultElement_();
        }
      });
    }
  }

  private onConfigurePin_(e: Event): void {
    e.preventDefault();
    recordLockScreenProgress(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  }

  private onSetupPinDialogClose_() {
    this.showSetupPinDialog_ = false;
    focusWithoutInk(
        castExists(this.shadowRoot!.querySelector('#setupPinButton')));
  }

  private onPinAutosubmitDialogClose_(): void {
    this.showPinAutosubmitDialog_ = false;
    focusWithoutInk(
        castExists(this.shadowRoot!.querySelector('#enablePinAutoSubmit')));
  }

  private onRecoveryDialogClose_(): void {
    this.showDisableRecoveryDialog_ = false;
    this.recoveryChangeInProcess_ = false;
    focusWithoutInk(
        castExists(this.shadowRoot!.querySelector('#recoveryToggle')));
  }

  /**
   * @param selectedUnlockType the current unlock type. Used to let
   *     Polymer know about the dependency.
   * @return true if the setup pin section should be shown.
   */
  private showConfigurePinButton_(selectedUnlockType: string): boolean {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  }

  private recoveryToggleSubLabel_(): string {
    if (!this.cryptohomeRecoveryEnabled_) {
      return '';
    }
    if (this.recovery_) {
      return this.i18n('recoveryToggleSubLabel');
    }
    return this.i18n('recoveryNotSupportedMessage');
  }

  private recoveryToggleLearnMoreUrl_(): string {
    if (!this.cryptohomeRecoveryEnabled_ || this.recovery_) {
      return '';
    }
    return this.i18n('recoveryNotSupportedMessage');
  }

  private recoveryToggleDisabled_(): boolean {
    if (!this.cryptohomeRecoveryEnabled_ || !this.recovery_) {
      return true;
    }
    return this.recoveryChangeInProcess_;
  }

  private recoveryTogglePref_(): chrome.settingsPrivate.PrefObject {
    if (this.recovery_) {
      return this.recovery_;
    }
    return this.noRecoveryVirtualPref_;
  }

  private getSetupPinText_(hasPin: boolean): string {
    if (hasPin) {
      return this.i18n('lockScreenChangePinButton');
    }
    return this.i18n('lockScreenSetupPinButton');
  }

  private updateNumFingerprintsDescription_(): void {
    if (this.numFingerprints_ === 0) {
      this.numFingerprintDescription_ =
          this.i18n('lockScreenEditFingerprintsDescription');
    } else {
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'lockScreenNumberFingerprints', this.numFingerprints_)
          .then(string => this.numFingerprintDescription_ = string);
    }
  }

  private onEditFingerprints_(): void {
    Router.getInstance().navigateTo(routes.FINGERPRINT);
  }

  /**
   * @return whether an event was fired to show the password dialog.
   */
  private requestPasswordIfApplicable_(): boolean {
    const currentRoute = Router.getInstance().currentRoute;
    if (currentRoute === routes.LOCK_SCREEN &&
        typeof this.authToken !== 'string') {
      const event = new CustomEvent(
          'password-requested', {bubbles: true, composed: true});
      this.dispatchEvent(event);
      return true;
    }
    return false;
  }

  private updateNumFingerprints_(): void {
    if (this.fingerprintUnlockEnabled_ && this.fingerprintBrowserProxy_) {
      this.fingerprintBrowserProxy_.getNumFingerprints().then(
          numFingerprints => {
            this.numFingerprints_ = numFingerprints;
          });
    }
  }

  /**
   * Looks up the translation id, which depends on PIN login support.
   */
  private selectLockScreenOptionsString(hasPinLogin: boolean): string {
    if (hasPinLogin) {
      return this.i18n('lockScreenOptionsLoginLock');
    }
    return this.i18n('lockScreenOptionsLock');
  }

  /**
   * Called by chrome when the state of an auth factor changes.
   * */
  onFactorChanged(factor: AuthFactor): void {
    switch (factor) {
      case AuthFactor.kRecovery:
        this.updateRecoveryState_(this.authToken);
        break;
      case AuthFactor.kPin:
        this.updatePinState_(this.authToken, /*factorChanged=*/ true);
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * Fetches state of an auth factor from the backend. Returns a |PrefObject|
   * suitable for use with a boolean toggle, or |null| if the auth factor is
   * not available.
   */
  private async fetchFactorState_(authFactor: AuthFactor):
      Promise<chrome.settingsPrivate.PrefObject|null> {
    assert(typeof this.authToken === 'string');

    const {supported} =
        await this.authFactorConfig.isSupported(this.authToken, authFactor);
    if (!supported) {
      return null;
    }

    // Fetch properties of the factor concurrently.
    const [{configured}, {management}, {editable}] = await Promise.all([
      this.authFactorConfig.isConfigured(this.authToken, authFactor),
      this.authFactorConfig.getManagementType(this.authToken, authFactor),
      this.authFactorConfig.isEditable(this.authToken, authFactor),
    ]);

    const state: chrome.settingsPrivate.PrefObject<boolean> = {
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: configured,
      key: '',
    };

    if (management !== ManagementType.kNone) {
      if (management === ManagementType.kDevice) {
        state.controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
      } else if (management === ManagementType.kChildRestriction) {
        state.controlledBy =
            chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION;
      } else {
        assert(management === ManagementType.kUser, 'Invalid management type');
        state.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
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
   */
  private async updateRecoveryState_(authToken: string|
                                     undefined): Promise<void> {
    if (!authToken) {
      return;
    }
    assert(authToken === this.authToken);
    this.recovery_ = await this.fetchFactorState_(AuthFactor.kRecovery);
  }

  /**
   * Fetches the state of the PIN factor and updates the corresponding
   * property.
   * @param authToken Must be equal to `this.authToken`. This parameter is
   *     passed as parameter so that this function can be used as callback for
   *     changes of the `authToken` property.
   * @param factorChanged Should be `true` if this function is called in
   *     response to a PIN change (as opposed to e.g. during initialization).
   */
  private async updatePinState_(
      authToken: string|undefined,
      factorChanged: boolean = false): Promise<void> {
    if (!authToken) {
      return;
    }
    assert(authToken === this.authToken);

    const {configured} = await this.authFactorConfig.isConfigured(
        this.authToken, AuthFactor.kPin);
    if (configured) {
      this.hasPin = true;
      this.selectedUnlockType = LockScreenUnlockType.PIN_PASSWORD;
      return;
    }
    assert(!configured);

    // A race condition can occur:
    // (1) User selects PIN_PASSSWORD, and successfully sets a pin, adding
    //     QuickUnlockMode.PIN to active modes.
    // (2) User selects PASSWORD, QuickUnlockMode.PIN capability is cleared
    //     from the active modes, notifying LockStateMixin to call
    //     updateUnlockType to fetch the active modes asynchronously.
    // (3) User selects PIN_PASSWORD, but the process from step 2 has not yet
    //     completed.
    // In this case, do not forcibly select the PASSWORD radio button even
    // though the unlock type is still PASSWORD (|hasPin| is false). If the
    // user wishes to set a pin, they will have to click the set pin button.
    // See https://crbug.com/1054327 for details.
    if (factorChanged && !this.hasPin &&
        this.selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD) {
      return;
    }
    this.hasPin = false;
    this.selectedUnlockType = LockScreenUnlockType.PASSWORD;
  }

  /**
   * Called when the user flips the recovery toggle.
   * @private
   */
  private async onRecoveryChange_(event: Event): Promise<void> {
    const target = event.target as SettingsToggleButtonElement;
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
      if (typeof this.authToken !== 'string') {
        fireAuthTokenInvalidEvent(this);
        return;
      }

      const {result} = await this.recoveryFactorEditor.configure(
          this.authToken, shouldEnable);
      switch (result) {
        case ConfigureResult.kSuccess:
          break;
        case ConfigureResult.kInvalidTokenError:
          // This will open the password prompt.
          fireAuthTokenInvalidEvent(this);
          return;
        case ConfigureResult.kFatalError:
          console.error('Error configuring recovery');
          return;
      }
    } finally {
      this.recoveryChangeInProcess_ = false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsLockScreenElement.is]: SettingsLockScreenElement;
  }
}

customElements.define(SettingsLockScreenElement.is, SettingsLockScreenElement);
