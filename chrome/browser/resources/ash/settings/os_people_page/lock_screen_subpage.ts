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

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import '../controls/settings_toggle_button.js';
import './setup_pin_dialog.js';
import './pin_autosubmit_dialog.js';
import './local_data_recovery_dialog.js';
import '../settings_shared.css.js';
import '../multidevice_page/multidevice_smartlock_item.js';
import './password_settings.js';
import './pin_settings.js';

import {fireAuthTokenInvalidEvent} from 'chrome://resources/ash/common/quick_unlock/utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {AuthFactor, ConfigureResult, FactorObserverReceiver, ManagementType} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {LockStateMixin} from '../lock_state_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
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

      /**
       * TODO(b/290916811): Whether to show a control for changing passwords.
       * Currently, we only show this if the user has a local password, but not
       * if the user has a Gaia password. Once the password-settings element
       * allows switching between types of passwords, we should always show
       * this control, making this flag obsolete.
       */
      showPasswordSettings_: {
        type: Boolean,
        value: false,
      },

      noRecoveryVirtualPref_: Object,

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
          Setting.kDataRecovery,
        ]),
      },

      /**
       * Whether switch from Gaia password factor to local password factor are
       * allowed by the feature flag.
       */
      changePasswordFactorSetupEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('changePasswordFactorSetupEnabled');
        },
        readOnly: true,
      },

      /**
       * Whether the device account is managed.
       */
      deviceAccountManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isDeviceAccountManaged');
        },
        readOnly: true,
      },
    };
  }

  prefs: Object;
  authToken: string|undefined;
  private fingerprintUnlockEnabled_: boolean;
  private numFingerprints_: number;
  private numFingerprintDescription_: string;
  private lockScreenNotificationsEnabled_: boolean;
  private lockScreenHideSensitiveNotificationSupported_: boolean;
  private recovery_: chrome.settingsPrivate.PrefObject|null;
  private noRecoveryVirtualPref_: chrome.settingsPrivate.PrefObject;
  private recoveryChangeInProcess_: boolean;
  private showPasswordSettings_: boolean;
  private showDisableRecoveryDialog_: boolean;
  private fingerprintBrowserProxy_: FingerprintBrowserProxy;
  private changePasswordFactorSetupEnabled_: boolean;
  private deviceAccountManaged_: boolean;

  static get observers() {
    return [
      'updateRecoveryState_(authToken)',
      'updatePasswordState_(authToken)',
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
    // TODO(crbug.com/40223898): Are we leaking |this| here because we never remove
    // the observer? We could close the pipe with |$.close()|, but not clear
    // whether that removes all references to |receiver| and then eventually to
    // |this|.
    const receiver = new FactorObserverReceiver(this);
    const remote = receiver.$.bindNewPipeAndPassRemote();
    this.authFactorConfig.observeFactorChanges(remote);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.updateNumFingerprints_();
  }

  override currentRouteChanged(newRoute: Route): void {
    if (newRoute === routes.LOCK_SCREEN) {
      this.updateNumFingerprints_();
      this.attemptDeepLink();
    }

    this.requestPasswordIfApplicable_();
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

  private async onAuthTokenChanged_(): Promise<void> {
    if (this.requestPasswordIfApplicable_()) {
      return;
    }

    if (Router.getInstance().currentRoute === routes.LOCK_SCREEN) {
      // Show deep links again if the user authentication dialog just closed.
      await this.attemptDeepLink();
    }
  }

  private onRecoveryDialogClose_(): void {
    this.showDisableRecoveryDialog_ = false;
    this.recoveryChangeInProcess_ = false;
    focusWithoutInk(
        castExists(this.shadowRoot!.querySelector('#recoveryToggle')));
  }

  private recoveryToggleSubLabel_(): string {
    if (this.recovery_) {
      return this.i18n('recoveryToggleSubLabel');
    }
    return this.i18n('recoveryNotSupportedMessage');
  }

  private recoveryToggleLearnMoreUrl_(): string {
    if (this.recovery_) {
      return '';
    }
    return this.i18n('recoveryLearnMoreUrl');
  }

  private recoveryToggleDisabled_(): boolean {
    if (!this.recovery_) {
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
      case AuthFactor.kGaiaPassword:
      case AuthFactor.kLocalPassword:
        this.updatePasswordState_(this.authToken);
        break;
      default:
        break;
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
   * Fetches the state of the password factor and updates the corresponding
   * property.
   * @param authToken Must be equal to |this.authToken|. The parameter is there
   *     so that this function can be used as callback for changes of the
   *     |authToken| property.
   */
  private async updatePasswordState_(authToken: string|
                                     undefined): Promise<void> {
    if (!authToken) {
      return;
    }
    assert(authToken === this.authToken);

    const [
      { configured: hasGaiaPassword },
      { configured: hasLocalPassword },
      { configured: hasPin },
    ] = await Promise.all([
      this.authFactorConfig.isConfigured(
        this.authToken, AuthFactor.kGaiaPassword),
      this.authFactorConfig.isConfigured(
        this.authToken, AuthFactor.kLocalPassword),
      this.authFactorConfig.isConfigured(
        this.authToken, AuthFactor.kPin),
    ]);

    if (hasLocalPassword) {
      // Local Password is the overriding factor here. We need to show change
      // option here.
      this.showPasswordSettings_ = true;
    } else if (!this.deviceAccountManaged_) {
      // Onto scenarios for non managed accounts now.
      if (this.changePasswordFactorSetupEnabled_ && hasGaiaPassword) {
        // If the gaia password is setup, for non managed users, we will allow
        // them to switch to local password.
        this.showPasswordSettings_ = true;
      } else if (!hasGaiaPassword && hasPin) {
        // At this point we know the user does not have a password
        // and has a pin. We can allow them to set password.
        this.showPasswordSettings_ = true;
      }
    } else {
      // This is a safety reset.
      this.showPasswordSettings_ = false;
    }
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
