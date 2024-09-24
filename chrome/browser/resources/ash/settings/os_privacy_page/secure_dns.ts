// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-secure-dns' is a setting that allows the secure DNS
 * mode and secure DNS resolvers to be configured.
 *
 * The underlying secure DNS prefs are not read directly since the setting is
 * meant to represent the current state of the host resolver, which depends not
 * only on the prefs but also a few other factors (e.g. whether we've detected a
 * managed environment, whether we've detected parental controls, etc). Instead,
 * the setting listens for secure-dns-setting-changed events, which are sent
 * by PrivacyPageBrowserProxy and describe the new host resolver configuration.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import '/shared/settings/prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import './secure_dns_input.js';
import './secure_dns_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl, ResolverOption, SecureDnsMode, SecureDnsSetting, SecureDnsUiManagementMode} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getTemplate} from './secure_dns.html.js';
import {SecureDnsInputElement} from './secure_dns_input.js';

export interface SettingsSecureDnsElement {
  $: {
    secureDnsInput: SecureDnsInputElement,
    secureDnsInputContainer: HTMLElement,
    resolverSelect: HTMLSelectElement,
  };
}

const SettingsSecureDnsElementBase =
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)));

/**
 * Enum for the categories of options in the secure DNS resolver select
 * menu.
 */
export enum SecureDnsResolverType {
  AUTOMATIC = 'automatic',
  BUILT_IN = 'built-in',
  CUSTOM = 'custom',
}

export class SettingsSecureDnsElement extends SettingsSecureDnsElementBase {
  static get is() {
    return 'settings-secure-dns';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Mirroring the secure DNS resolver enum so that it can be used from HTML
       * bindings.
       */
      resolverTypeEnum_: {
        type: Object,
        value: SecureDnsResolverType,
      },

      /**
       * The setting sublabel.
       */
      secureDnsDescription_: String,

      /**
       * Represents whether the main toggle for the secure DNS setting is
       * switched on or off.
       */
      secureDnsToggle_: {
        type: Object,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      /**
       * Whether the secure DNS resolver options should be shown.
       */
      showSecureDnsOptions_: Boolean,

      /**
       * List of secure DNS resolvers to display in dropdown menu.
       */
      resolverOptions_: Array,

      /**
       * String displaying the privacy policy of the resolver selected in the
       * dropdown menu.
       */
      privacyPolicyString_: String,

      /**
       * String to display in the custom text field.
       */
      secureDnsInputValue_: String,

      showDisableDnsDialog_: {
        type: Boolean,
        value: false,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isRevampWayfindingEnabled');
        },
        readOnly: true,
      },

      isDeprecateDnsDialogEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isDeprecateDnsDialogEnabled');
        },
        readOnly: true,
      },

      shouldShowDialogWhenDisablingDns_: {
        type: Boolean,
        computed: 'computeShouldShowDialogWhenDisablingDns_(' +
            'isDeprecateDnsDialogEnabled_, isRevampWayfindingEnabled_)',
      },

      /**
       * Boolean to make network default description visible if user selects
       * Automatic option in DNS dropdown.
       */
      showNetworkDefaultDescription_: {
        type: Boolean,
        value: false,
      },

      /**
       * Boolean to make privacy policy description visible if user selects a
       * Secure option in DNS dropdown.
       */
      showPrivacyPolicyDescription_: {
        type: Boolean,
        value: false,
      },

      networkDefaultAriaDescribedBy_: {
        type: String,
        value: null,
      },
    };
  }

  private secureDnsDescription_: string;
  private secureDnsToggle_: chrome.settingsPrivate.PrefObject<boolean>;
  private showSecureDnsOptions_: boolean;
  private resolverOptions_: ResolverOption[];
  private privacyPolicyString_: TrustedHTML;
  private secureDnsInputValue_: string;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();
  private showDisableDnsDialog_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private isDeprecateDnsDialogEnabled_: boolean;
  private shouldShowDialogWhenDisablingDns_: boolean;
  private showNetworkDefaultDescription_: boolean;
  private showPrivacyPolicyDescription_: boolean;
  private networkDefaultAriaDescribedBy_: string|null;


  override connectedCallback(): void {
    super.connectedCallback();

    // Fetch the options for the dropdown menu before configuring the setting
    // to match the underlying host resolver configuration.
    this.browserProxy_.getSecureDnsResolverList().then(resolvers => {
      this.resolverOptions_ = resolvers;
      this.browserProxy_.getSecureDnsSetting().then(
          (setting: SecureDnsSetting) =>
              this.onSecureDnsPrefsChanged_(setting));

      // Listen to changes in the host resolver configuration and update the
      // UI representation to match. (Changes to the host resolver configuration
      // may be generated in ways other than direct UI manipulation).
      this.addWebUiListener(
          'secure-dns-setting-changed',
          (setting: SecureDnsSetting) =>
              this.onSecureDnsPrefsChanged_(setting));

      // This event will only get dispatched from the DNS dialog. If the flag
      // kOsSettingsDeprecateDnsDialog is enabled, we don't have to listen for
      // the event.
      if (this.shouldShowDialogWhenDisablingDns_) {
        this.addEventListener(
            'dns-settings-invalid-custom-to-off-mode',
            () => this.onSecureDnsPrefChangedToFalse_());
      }
    });
  }

  private setDropdownDescriptionVisibility_(
      networkDefault: boolean, privacyPolicy: boolean): void {
    this.showNetworkDefaultDescription_ = networkDefault;
    this.showPrivacyPolicyDescription_ = privacyPolicy;

    this.networkDefaultAriaDescribedBy_ =
        networkDefault ? 'networkDefaultDescription' : null;
  }

  // Hide DNS dropdown description strings.
  private hideDropdownDescriptions_(): void {
    this.setDropdownDescriptionVisibility_(
        /*networkDefault=*/ false, /*privacyPolicy=*/ false);
  }

  /**
   * Update the UI representation to match the underlying host resolver
   * configuration.
   */
  private onSecureDnsPrefsChanged_(setting: SecureDnsSetting): void {
    switch (setting.osMode) {
      case SecureDnsMode.SECURE:
      case SecureDnsMode.AUTOMATIC:
        this.set('secureDnsToggle_.value', true);
        this.updateConfigRepresentation_(setting.osMode, setting.osConfig);
        break;
      case SecureDnsMode.OFF:
        this.set('secureDnsToggle_.value', false);
        this.hideDropdownDescriptions_();
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }

    this.updateManagementView_(setting);
  }

  private onSecureDnsPrefChangedToFalse_(): void {
    this.set('secureDnsToggle_.value', false);
    this.showSecureDnsOptions_ = false;
  }

  /**
   * Updates the underlying secure DNS mode pref based on the new toggle
   * selection (and the underlying select menu if the toggle has just been
   * turned on).
   */
  private onToggleChanged_(): void {
    this.showSecureDnsOptions_ = this.secureDnsToggle_.value;

    if (!this.secureDnsToggle_.value) {
      this.updateDnsPrefs_(SecureDnsMode.OFF);
      return;
    }

    const resolver = this.$.resolverSelect.value;
    if (resolver === SecureDnsResolverType.AUTOMATIC) {
      this.updateDnsPrefs_(SecureDnsMode.AUTOMATIC);
    } else {
      if (resolver === SecureDnsResolverType.CUSTOM) {
        this.$.secureDnsInput.focus();
      }
      this.updateDnsPrefs_(SecureDnsMode.SECURE);
    }
  }

  /**
   * Only gets called when the user wants to turn on the toggle from ChromeOS
   * Settings.
   */
  private turnOnDnsToggle_(): void {
    this.set('secureDnsToggle_.value', true);
    this.onToggleChanged_();
  }

  /**
   * Helper method for updating the underlying secure DNS prefs based on the
   * provided mode and templates (if the latter is specified). The templates
   * param should only be specified when the underlying prefs are being updated
   * after a custom entry has been validated.
   */
  private updateDnsPrefs_(mode: SecureDnsMode, templates: string = ''): void {
    switch (mode) {
      case SecureDnsMode.SECURE:
        // If going to secure mode, set the templates pref first to prevent the
        // stub resolver config from being momentarily invalid. If the user has
        // selected the custom dropdown option, only update the underlying
        // prefs if the templates param was specified. If the templates param
        // was not specified, the custom entry may be invalid or may not
        // have passed validation yet, and we should not update either the
        // underlying mode or templates prefs.
        const builtInResolver = this.builtInResolver_();
        if (!builtInResolver) {
          if (!templates) {
            return;
          }
          this.setPrefValue('dns_over_https.templates', templates);
        } else {
          this.setPrefValue('dns_over_https.templates', builtInResolver.value);
        }
        this.setPrefValue('dns_over_https.mode', mode);
        break;
      case SecureDnsMode.AUTOMATIC:
      case SecureDnsMode.OFF:
        // If going to automatic or off mode, set the mode pref first to avoid
        // clearing the dropdown selection when the templates pref is cleared.
        this.setPrefValue('dns_over_https.mode', mode);
        this.setPrefValue('dns_over_https.templates', '');
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }
  }

  /**
   * Updates the underlying secure DNS templates pref based on the selected
   * resolver and displays the corresponding privacy policy.
   */
  private onDropdownSelectionChanged_(): void {
    switch (this.$.resolverSelect.value) {
      case SecureDnsResolverType.AUTOMATIC:
        this.updateDnsPrefs_(SecureDnsMode.AUTOMATIC);
        this.updateConfigRepresentation_(SecureDnsMode.AUTOMATIC, '');
        break;
      case SecureDnsResolverType.CUSTOM:
        this.updateDnsPrefs_(SecureDnsMode.SECURE);
        this.updateConfigRepresentation_(SecureDnsMode.SECURE, '');
        break;
      default:
        const resolver = this.builtInResolver_();
        assert(resolver);
        this.updateDnsPrefs_(SecureDnsMode.SECURE, resolver.value);
        this.updateConfigRepresentation_(SecureDnsMode.SECURE, resolver.value);
        break;
    }
  }

  /**
   * Updates the setting to communicate the type of management, if any. The
   * setting is always collapsed if there is any management.
   */
  private updateManagementView_(setting: SecureDnsSetting): void {
    if (this.prefs === undefined) {
      return;
    }
    // If the underlying secure DNS mode pref has an enforced value, communicate
    // that via the toggle pref.
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: this.secureDnsToggle_.value,
    };

    // The message to be displayed when the device is managed. On Chrome OS, if
    // the effective template URI contains identifiers (which are
    // hashed with a salt and hex encoded), then the message will contain the
    // template URI for display in which the identifiers are shown in plain
    // text.
    let secureDescription =
        loadTimeData.getString('secureDnsOsSettingsDescription');

    if (setting.dohWithIdentifiersActive) {
      secureDescription = loadTimeData.substituteString(
          loadTimeData.getString('secureDnsWithIdentifiersDescription'),
          setting.configForDisplay);
    }

    if (setting.osMode !== SecureDnsMode.OFF && setting.dohDomainConfigSet) {
      secureDescription =
          loadTimeData.getString('secureDnsWithDomainConfigDescription');
      if (setting.dohWithIdentifiersActive) {
        secureDescription = loadTimeData.substituteString(
            loadTimeData.getString(
                'secureDnsWithIdentifiersAndDomainConfigDescription'),
            setting.configForDisplay);
      }
    }

    if (this.getPref('dns_over_https.mode').enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = this.getPref('dns_over_https.mode').controlledBy;
      this.secureDnsDescription_ = secureDescription;
    } else {
      // If the secure DNS mode was forcefully overridden by Chrome, provide an
      // explanation in the setting subtitle.
      switch (setting.managementMode) {
        case SecureDnsUiManagementMode.NO_OVERRIDE:
          this.secureDnsDescription_ = secureDescription;
          break;
        case SecureDnsUiManagementMode.DISABLED_MANAGED:
          pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
          this.secureDnsDescription_ =
              loadTimeData.getString('secureDnsDisabledForManagedEnvironment');
          break;
        case SecureDnsUiManagementMode.DISABLED_PARENTAL_CONTROLS:
          pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
          this.secureDnsDescription_ =
              loadTimeData.getString('secureDnsDisabledForParentalControl');
          break;
        default:
          assertNotReached(
              'Received unknown secure DNS management mode ' +
              setting.managementMode);
      }
    }
    this.secureDnsToggle_ = pref;

    if (this.secureDnsToggle_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      this.showSecureDnsOptions_ = false;
    } else {
      this.showSecureDnsOptions_ = this.secureDnsToggle_.value;
    }
  }

  /**
   * Updates the UI to match the provided configuration parameters.
   */
  private updateConfigRepresentation_(mode: SecureDnsMode, template: string):
      void {
    let hideCustomEntry = true;
    let selectValue = '';
    let privacyPolicy = '';

    const index = this.resolverOptions_.findIndex(r => r.value === template);
    if (index !== -1) {
      privacyPolicy = this.resolverOptions_[index].policy;
    }

    switch (mode) {
      case SecureDnsMode.AUTOMATIC:
        selectValue = SecureDnsResolverType.AUTOMATIC;
        this.setDropdownDescriptionVisibility_(
            /*networkDefault=*/ true, /*privacyPolicy=*/ false);
        break;
      case SecureDnsMode.SECURE:
        if (index === -1) {
          selectValue = SecureDnsResolverType.CUSTOM;
          hideCustomEntry = false;
          this.hideDropdownDescriptions_();
        } else {
          selectValue = index.toString();
        }
        break;
      default:
        assertNotReached(`Unexpected DNS mode ${mode}`);
    }

    this.$.resolverSelect.value = selectValue;

    this.updatePrivacyPolicyLine_(privacyPolicy);

    this.$.secureDnsInputContainer.hidden = hideCustomEntry;
    if (!hideCustomEntry) {
      this.secureDnsInputValue_ = template;
      if (!template) {
        this.$.secureDnsInput.focus();
      }
    }
  }

  /**
   * Displays the privacy policy string if the policy URL is specified.
   * @param policy The privacy policy URL.
   */
  private updatePrivacyPolicyLine_(policy: string): void {
    // There is no privacy policy description for the custom resolver and
    // automatic options.
    if (!policy) {
      return;
    }

    // Display the corresponding privacy policy.
    this.privacyPolicyString_ = this.i18nAdvanced(
        'secureDnsSecureDropdownModePrivacyPolicy', {substitutions: [policy]});
    this.setDropdownDescriptionVisibility_(
        /*networkDefault=*/ false, /*privacyPolicy=*/ true);
  }

  /**
   * Updates the underlying prefs if a custom entry was determined to be valid.
   */
  private onSecureDnsInputEvaluated_(
      event: CustomEvent<{text: string, isValid: boolean}>): void {
    if (event.detail.isValid) {
      this.updateDnsPrefs_(SecureDnsMode.SECURE, event.detail.text);
    }
  }

  /**
   * Returns the ResolverOption details if the currently selected secure DNS
   * resolver is a built-in one.
   */
  private builtInResolver_(): ResolverOption|undefined {
    if (this.$.resolverSelect.selectedOptions[0].dataset['resolverType'] ===
        SecureDnsResolverType.BUILT_IN) {
      const index = Number.parseInt(this.$.resolverSelect.value);
      return this.resolverOptions_[index];
    }
    return undefined;
  }

  private onDnsToggleClick_(): void {
    const secureDnsToggle =
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#secureDnsToggle');
    assert(secureDnsToggle);
    if (secureDnsToggle.checked) {
      // Always allow turning on the toggle.
      this.turnOnDnsToggle_();
      return;
    }

    // Do not update the underlying pref value to false. Instead if the user is
    // attempting to turn off the toggle, present the warning dialog.
    this.showDisableDnsDialog_ = true;
    return;
  }

  private onDisableDnsDialogClosed_(): void {
    // Sync the toggle's value to its pref value.
    const secureDnsToggle =
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#secureDnsToggle');
    assert(secureDnsToggle);
    secureDnsToggle.resetToPrefValue();

    this.showDisableDnsDialog_ = false;
  }

  private getDnsRowIcon_(): string {
    return this.isRevampWayfindingEnabled_ ? 'os-settings:privacy-secure-dns' :
                                             '';
  }

  /**
   * The DNS Dialog has been deprecated, so we will only show the dialog when
   * the feature flag kOsSettingsDeprecateDnsDialog is disabled and revamp flag
   * is enabled.
   *
   * Returns whether we should show the DNS dialog when the user toggles DNS
   * from enabled to disabled.
   */
  private computeShouldShowDialogWhenDisablingDns_(): boolean {
    return !this.isDeprecateDnsDialogEnabled_ &&
        this.isRevampWayfindingEnabled_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-secure-dns': SettingsSecureDnsElement;
  }
}

customElements.define(SettingsSecureDnsElement.is, SettingsSecureDnsElement);
