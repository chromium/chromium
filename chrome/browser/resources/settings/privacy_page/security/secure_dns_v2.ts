// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * NOTE: This is currently forked over from the original settings-secure-dns.
 * @fileoverview 'settings-secure-dns-v2' is a setting that allows the secure
 * DNS mode and secure DNS resolvers to be configured.
 *
 * The underlying secure DNS prefs are not read directly since the setting is
 * meant to represent the current state of the host resolver, which depends not
 * only on the prefs but also a few other factors (e.g. whether we've detected a
 * managed environment, whether we've detected parental controls, etc). Instead,
 * the setting listens for secure-dns-setting-changed events, which are sent
 * by PrivacyPageBrowserProxy and describe the new host resolver configuration.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '/shared/settings/prefs/prefs.js';
import './security_page_feature_row.js';
import '../../controls/controlled_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './secure_dns_input.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {PrivacyPageBrowserProxy, ResolverOption, SecureDnsSetting} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import type {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ControlledRadioButtonElement} from '../../controls/controlled_radio_button.js';
import {loadTimeData} from '../../i18n_setup.js';

import type {SecureDnsInputElement} from './secure_dns_input.js';
import {getTemplate} from './secure_dns_v2.html.js';
import type {SecurityPageFeatureRowElement} from './security_page_feature_row.js';

export interface SettingsSecureDnsV2Element {
  $: {
    automaticRadioButton: ControlledRadioButtonElement,
    customRadioButton: ControlledRadioButtonElement,
    fallbackRadioButton: ControlledRadioButtonElement,
    featureRow: SecurityPageFeatureRowElement,
    privacyPolicy: HTMLElement,
    secureDnsInput: SecureDnsInputElement,
    secureDnsInputContainer: HTMLElement,
    radioGroup: CrRadioGroupElement,
    resolverSelect: HTMLSelectElement,
  };
}

const SettingsSecureDnsV2ElementBase =
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)));

/**
 * Enum for the categories of options in the secure DNS resolver select
 * menu and the radio button group.
 */
export enum SecureDnsV2ResolverType {
  AUTOMATIC = 'automatic',
  FALLBACK = 'fallback',
  BUILT_IN = 'built-in',
  CUSTOM = 'custom',
}

export class SettingsSecureDnsV2Element extends SettingsSecureDnsV2ElementBase {
  static get is() {
    return 'settings-secure-dns-v2';
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
        value: SecureDnsV2ResolverType,
      },

      /**
       * The setting sublabel.
       */
      secureDnsDescription_: String,

      /**
       * Represents whether the main toggle for the secure DNS setting is
       * switched on or off.
       */
      secureDnsTogglePref_: {
        type: Object,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      /**
       * List of secure DNS resolvers to display in dropdown menu.
       */
      resolverOptions_: {
        type: Array,
        value: () => [],
      },

      /**
       * String displaying the privacy policy of the resolver selected in the
       * dropdown menu.
       */
      privacyPolicyString_: String,

      /**
       * String to display in the custom text field.
       */
      secureDnsInputValue_: String,

      /**
       * Helper array to map the OFF mode to the unchecked state of the toggle.
       * Used by security-page-feature-row.
       */
      secureDnsModeUncheckedValues_: {
        type: Array,
        value: () => [SecureDnsMode.OFF],
      },

      /**
       * Tracks which radio button is currently selected.
       * Values: 'automatic', 'fallback', or 'custom'.
       */
      selectedMode_: String,

      isSecureDnsWarningIconVisible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Controls visibility of the custom input container.
       * Replaces the old 'showSecureDnsOptions_' logic.
       */
      showCustomInput_: Boolean,

      /**
       * State text map to map each radio button to a string.
       */
      secureDnsStateTextMap_: {
        type: Object,
        value: () => ({
          [SecureDnsV2ResolverType.AUTOMATIC]:
              loadTimeData.getString('securityFeatureRowStateStandard'),
          [SecureDnsV2ResolverType.FALLBACK]:
              loadTimeData.getString('securityFeatureRowStateEnhanced'),
          [SecureDnsV2ResolverType.CUSTOM]:
              loadTimeData.getString('securityFeatureRowStateEnhancedCustom'),
          [SecureDnsMode.OFF]:
              loadTimeData.getString('securityFeatureRowStateOff'),
        }),
      },

      /**
       * Computed key for the feature row text.
       * Automatically updates when the toggle or selection changes.
       */
      computedStateKey_: {
        type: String,
        computed: 'computeStateKey_(secureDnsTogglePref_.value, selectedMode_)',
      },
    };
  }

  static get observers() {
    return [
      // Observes the fallback pref to fix the race condition when switching
      // bundles.
      'onFallbackPrefChanged_(prefs.dns_over_https.automatic_mode_fallback_to_doh.value)',
    ];
  }

  declare private resolverTypeEnum_: object;
  declare private secureDnsDescription_: string;
  declare private secureDnsTogglePref_:
      chrome.settingsPrivate.PrefObject<boolean>;
  declare private resolverOptions_: ResolverOption[];
  declare private privacyPolicyString_: TrustedHTML;
  declare private secureDnsInputValue_: string;
  declare private secureDnsModeUncheckedValues_: SecureDnsMode[];
  declare private selectedMode_: SecureDnsV2ResolverType;
  declare private isSecureDnsWarningIconVisible_: boolean;
  declare private showCustomInput_: boolean;
  declare private secureDnsStateTextMap_: object;
  declare private computedStateKey_: string;

  private lastSelected_: SecureDnsV2ResolverType|undefined;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    // Fetch the options for the dropdown menu.
    this.browserProxy_.getSecureDnsResolverList().then(resolvers => {
      this.resolverOptions_ = resolvers;
      this.browserProxy_.getSecureDnsSetting().then(
          (setting: SecureDnsSetting) =>
              this.onSecureDnsPrefsChanged_(setting));

      this.addWebUiListener(
          'secure-dns-setting-changed',
          (setting: SecureDnsSetting) =>
              this.onSecureDnsPrefsChanged_(setting));
    });
  }

  /**
   * Observer for the fallback pref. Ensures UI updates correctly even if
   * the event listener fires before the PrefsMixin update.
   */
  private onFallbackPrefChanged_() {
    // Re-run the pref change logic to catch the updated fallback value.
    this.browserProxy_.getSecureDnsSetting().then(
        (setting: SecureDnsSetting) => this.onSecureDnsPrefsChanged_(setting));
  }

  private computeStateKey_(): string {
    // If the toggle is visually off, we must show the "Off" text.
    if (!this.secureDnsTogglePref_.value) {
      return SecureDnsMode.OFF;
    }
    // Otherwise, show the text corresponding to the selected radio button.
    return this.selectedMode_;
  }

  /**
   * Update the UI representation to match the underlying host resolver
   * configuration.
   */
  private onSecureDnsPrefsChanged_(setting: SecureDnsSetting) {
    switch (setting.mode) {
      case SecureDnsMode.SECURE:
        this.set('secureDnsTogglePref_.value', true);
        this.selectedMode_ = SecureDnsV2ResolverType.CUSTOM;
        this.updateConfigRepresentation_(setting.mode, setting.config);
        break;
      case SecureDnsMode.AUTOMATIC:
        this.set('secureDnsTogglePref_.value', true);
        const fallbackEnabled =
            this.getPref('dns_over_https.automatic_mode_fallback_to_doh').value;
        if (fallbackEnabled) {
          this.selectedMode_ = SecureDnsV2ResolverType.FALLBACK;
        } else {
          this.selectedMode_ = SecureDnsV2ResolverType.AUTOMATIC;
        }
        this.updateConfigRepresentation_(setting.mode, setting.config);
        break;
      case SecureDnsMode.OFF:
        this.selectedMode_ = SecureDnsV2ResolverType.AUTOMATIC;
        this.set('secureDnsTogglePref_.value', false);
        break;
      default:
        assertNotReachedCase(
            setting.mode,
            'Secure DNS pref contains unknown mode: ' + setting.mode);
    }

    // Update `lastSelected_` here to help filter out the
    // selected-changed event triggered by the above pref update in
    // `onRadioGroupChange_()`.
    this.lastSelected_ = this.selectedMode_;
    this.updateManagementView_(setting);
    this.updateAppearance_();
  }

  /**
   * Handles user clicks on the Radio Buttons.
   * Because of 'no-set-pref', we manually trigger the backend update.
   */
  private onRadioGroupChange_(event: CustomEvent<{value: string}>) {
    // Ignore events that were triggered by changes to the toggle or underlying
    // prefs.
    if (this.lastSelected_ === event.detail.value) {
      return;
    }
    this.lastSelected_ = event.detail.value as SecureDnsV2ResolverType;

    // If the toggle is OFF, clicking a radio button should turn it ON.
    if (this.getPref('dns_over_https.mode').enforcement !==
            chrome.settingsPrivate.Enforcement.ENFORCED &&
        !this.secureDnsTogglePref_.value) {
      this.set('secureDnsTogglePref_.value', true);
    }

    this.applySelectionToPrefs_();

    this.dispatchEvent(new CustomEvent('radio-group-change', {
      bubbles: true,
      composed: true,
      detail: {value: this.$.radioGroup.selected},
    }));
  }


  /**
   * Updates the underlying secure DNS mode pref based on the new toggle
   * selection.
   */
  private onToggleButtonChange_() {
    if (!this.secureDnsTogglePref_.value) {
      this.lastSelected_ = SecureDnsV2ResolverType.AUTOMATIC;
      this.selectedMode_ = SecureDnsV2ResolverType.AUTOMATIC;
      this.updateDnsPrefs_(SecureDnsMode.OFF);
      return;
    }

    this.updateAppearance_();
    // Restore the state based on the current radio selection
    this.applySelectionToPrefs_();
  }

  /**
   * Updates the underlying secure DNS preferences based on the new radio group
   *  selection.
   */
  private applySelectionToPrefs_() {
    // Get currently selected radio button.
    const selected = this.$.radioGroup.selected as SecureDnsV2ResolverType;

    switch (selected) {
      case SecureDnsV2ResolverType.AUTOMATIC:
        this.updateDnsPrefs_(
            SecureDnsMode.AUTOMATIC, /*templates=*/ '', /*fallback=*/ false);
        break;
      case SecureDnsV2ResolverType.FALLBACK:
        this.updateDnsPrefs_(
            SecureDnsMode.AUTOMATIC, /*templates=*/ '', /*fallback=*/ true);
        break;
      case SecureDnsV2ResolverType.CUSTOM:
      case SecureDnsV2ResolverType.BUILT_IN:
        this.updateDnsPrefs_(
            SecureDnsMode.SECURE, this.getSecureDnsResolverTemplate_());
        this.onResolverSelectChange_();
        break;
      default:
        assertNotReachedCase(
            selected, 'Unknown secure DNS selection made: ' + selected);
    }
  }

  /**
   * Helper function to determine the secure DNS template to use when the
   * "Secure" radio button is selected.
   */
  private getSecureDnsResolverTemplate_(): string {
    const builtIn = this.builtInResolver_();
    if (builtIn) {
      return builtIn.value;
    }

    if (this.secureDnsInputValue_ && this.secureDnsInputValue_.trim() !== '') {
      return this.secureDnsInputValue_;
    }

    // Fallback: Default to first resolver (e.g. Google DNS) if input is empty.
    if (this.resolverOptions_.length > 0) {
      this.$.resolverSelect.value = '0';
      return this.resolverOptions_[0].value;
    }

    return '';
  }

  /**
   * Helper method for updating the underlying secure DNS prefs.
   * Updated to handle the new fallback preference.
   */
  private updateDnsPrefs_(
      mode: SecureDnsMode, templates: string = '', fallback: boolean = false) {
    switch (mode) {
      case SecureDnsMode.SECURE:
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
        // Ensure fallback is disabled in Secure mode
        this.setPrefValue(
            'dns_over_https.automatic_mode_fallback_to_doh', false);
        break;

      case SecureDnsMode.AUTOMATIC:
        this.setPrefValue('dns_over_https.mode', mode);
        this.setPrefValue('dns_over_https.templates', '');
        this.setPrefValue(
            'dns_over_https.automatic_mode_fallback_to_doh', fallback);
        break;

      case SecureDnsMode.OFF:
        this.setPrefValue('dns_over_https.mode', mode);
        this.setPrefValue('dns_over_https.templates', '');
        this.setPrefValue(
            'dns_over_https.automatic_mode_fallback_to_doh', false);
        break;

      default:
        assertNotReachedCase(mode, 'Received unknown secure DNS mode');
    }
  }

  /**
   * Updates the underlying secure DNS templates pref based on the selected
   * resolver.
   */
  private onResolverSelectChange_(event?: Event) {
    // Prevent the event from propagating to the settings-radio-group.
    // Handle the event here instead.
    if (event) {
      event.stopPropagation();
    }

    // "Automatic" is no longer in the dropdown.
    if (this.$.resolverSelect.value === SecureDnsV2ResolverType.CUSTOM) {
      this.updateDnsPrefs_(SecureDnsMode.SECURE);
      this.updateConfigRepresentation_(SecureDnsMode.SECURE, '');
      return;
    }

    const resolver = this.builtInResolver_();
    if (resolver) {
      this.updateDnsPrefs_(SecureDnsMode.SECURE, resolver.value);
      this.updateConfigRepresentation_(SecureDnsMode.SECURE, resolver.value);
    }
  }

  /**
   * Updates the setting to communicate the type of management, if any.
   */
  private updateManagementView_(setting: SecureDnsSetting) {
    if (this.prefs === undefined) {
      return;
    }
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: this.secureDnsTogglePref_.value,
    };

    const secureDescription = loadTimeData.getString('secureDnsDescription');

    if (this.getPref('dns_over_https.mode').enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = this.getPref('dns_over_https.mode').controlledBy;
      this.secureDnsDescription_ = secureDescription;
    } else {
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
          assertNotReachedCase(
              setting.managementMode,
              'Received unknown secure DNS management mode ' +
                  setting.managementMode);
      }
    }
    this.secureDnsTogglePref_ = pref;
  }

  private updateAppearance_() {
    if (this.prefs === undefined) {
      return;
    }

    const enforced = this.secureDnsTogglePref_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;

    this.$.automaticRadioButton.disabled = enforced;
    this.$.fallbackRadioButton.disabled = enforced;
    this.$.customRadioButton.disabled = enforced;

    this.isSecureDnsWarningIconVisible_ =
        !this.secureDnsTogglePref_.value && !enforced;
  }

  /**
   * Updates the UI to match the provided configuration parameters.
   */
  private updateConfigRepresentation_(mode: SecureDnsMode, template: string) {
    let selectValue = '';
    let privacyPolicy = '';
    let showInput = false;

    const index = this.resolverOptions_.findIndex(r => r.value === template);
    if (index !== -1) {
      privacyPolicy = this.resolverOptions_[index].policy;
    }

    switch (mode) {
      case SecureDnsMode.AUTOMATIC:
        // Don't touch the dropdown. Just hide custom input.
        showInput = false;
        break;
      case SecureDnsMode.SECURE:
        if (index === -1) {
          selectValue = SecureDnsV2ResolverType.CUSTOM;
          showInput = true;
        } else {
          selectValue = index.toString();
          showInput = false;
        }
        this.$.resolverSelect.value = selectValue;
        break;
      case SecureDnsMode.OFF:
        break;
      default:
        assertNotReachedCase(
            mode, 'Configuration has unknown secure DNS mode: ' + mode);
    }

    this.updatePrivacyPolicyLine_(privacyPolicy);

    this.showCustomInput_ = showInput;
    if (showInput) {
      this.secureDnsInputValue_ = template;
      if (!template) {
        this.$.secureDnsInput.focus();
      }
    }
  }

  private updatePrivacyPolicyLine_(policy: string) {
    if (!policy) {
      this.$.privacyPolicy.style.display = 'none';
      return;
    }
    this.$.privacyPolicy.style.display = 'block';
    this.privacyPolicyString_ = sanitizeInnerHtml(loadTimeData.substituteString(
        loadTimeData.getString('secureDnsSecureDropdownModePrivacyPolicy'),
        policy));
  }

  private onSecureDnsInputValueUpdate_(
      event: CustomEvent<{text: string, isValid: boolean}>) {
    if (event.detail.isValid) {
      this.updateDnsPrefs_(SecureDnsMode.SECURE, event.detail.text);
    }
  }

  private builtInResolver_(): ResolverOption|undefined {
    if (this.$.resolverSelect.selectedOptions[0].dataset['resolverType'] ===
        SecureDnsV2ResolverType.BUILT_IN) {
      const index = Number.parseInt(this.$.resolverSelect.value);
      return this.resolverOptions_[index];
    }
    return undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-secure-dns-v2': SettingsSecureDnsV2Element;
  }
}

customElements.define(
    SettingsSecureDnsV2Element.is, SettingsSecureDnsV2Element);
