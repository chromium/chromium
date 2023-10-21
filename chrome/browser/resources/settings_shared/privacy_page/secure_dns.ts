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

import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import './secure_dns_input.js';
// <if expr="chromeos_ash">
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import './secure_dns_dialog.js';

// </if>

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl, ResolverOption, SecureDnsMode, SecureDnsSetting, SecureDnsUiManagementMode} from './privacy_page_browser_proxy.js';
import {getTemplate} from './secure_dns.html.js';
import {SecureDnsInputElement} from './secure_dns_input.js';

export interface SettingsSecureDnsElement {
  $: {
    privacyPolicy: HTMLElement,
    secureDnsInput: SecureDnsInputElement,
    secureDnsRadioGroup: CrRadioGroupElement,
    secureResolverSelect: HTMLSelectElement,
  };
}

const SettingsSecureDnsElementBase =
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)));

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
       * Mirroring the secure DNS mode enum so that it can be used from HTML
       * bindings.
       */
      secureDnsModeEnum_: {
        type: Object,
        value: SecureDnsMode,
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
       * Whether the radio buttons should be shown.
       */
      showRadioGroup_: Boolean,

      /**
       * Represents the selected radio button. Should always have a value of
       * 'automatic' or 'secure'.
       */
      secureDnsRadio_: {
        type: String,
        value: SecureDnsMode.AUTOMATIC,
      },

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

      // <if expr="chromeos_ash">
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
      // </if>
    };
  }

  private secureDnsDescription_: string;
  private secureDnsToggle_: chrome.settingsPrivate.PrefObject<boolean>;
  private showRadioGroup_: boolean;
  private secureDnsRadio_: SecureDnsMode;
  private resolverOptions_: ResolverOption[];
  private privacyPolicyString_: TrustedHTML;
  private secureDnsInputValue_: string;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();
  // <if expr="chromeos_ash">
  private showDisableDnsDialog_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  // </if>

  override connectedCallback() {
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

      // <if expr="chromeos_ash">
      this.addEventListener(
          'dns-settings-invalid-custom-to-off-mode',
          () => this.onSecureDnsPrefChangedToFalse_());
      // </if>
    });
  }

  /**
   * Update the UI representation to match the underlying host resolver
   * configuration.
   */
  private onSecureDnsPrefsChanged_(setting: SecureDnsSetting) {
    switch (setting.mode) {
      case SecureDnsMode.SECURE:
        this.set('secureDnsToggle_.value', true);
        this.secureDnsRadio_ = SecureDnsMode.SECURE;
        // Only update the selected dropdown item if the user is in secure
        // mode. Otherwise, we may be losing a selection that hasn't been
        // pushed yet to prefs.
        this.updateConfigRepresentation_(setting.config);
        this.updatePrivacyPolicyLine_();
        break;
      case SecureDnsMode.AUTOMATIC:
        this.set('secureDnsToggle_.value', true);
        this.secureDnsRadio_ = SecureDnsMode.AUTOMATIC;
        break;
      case SecureDnsMode.OFF:
        this.set('secureDnsToggle_.value', false);
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }

    this.updateManagementView_(setting);
  }

  // <if expr="chromeos_ash">
  private onSecureDnsPrefChangedToFalse_() {
    this.set('secureDnsToggle_.value', false);
    this.showRadioGroup_ = false;
  }
  // </if>

  /**
   * Updates the underlying secure DNS mode pref based on the new toggle
   * selection (and the underlying radio button if the toggle has just been
   * turned on).
   */
  private onToggleChanged_() {
    this.showRadioGroup_ = this.secureDnsToggle_.value;
    if (this.secureDnsRadio_ === SecureDnsMode.SECURE &&
        !this.$.secureResolverSelect.value) {
      this.$.secureDnsInput.focus();
    }
    this.updateDnsPrefs_(
        this.secureDnsToggle_.value ? this.secureDnsRadio_ : SecureDnsMode.OFF);
  }

  // <if expr="chromeos_ash">
  /**
   * Only gets called when the user wants to turn on the toggle from ChromeOS
   * Settings.
   */
  private turnOnDnsToggle_() {
    this.showRadioGroup_ = true;
    if (this.secureDnsRadio_ === SecureDnsMode.SECURE &&
        !this.$.secureResolverSelect.value) {
      this.$.secureDnsInput.focus();
    }
    this.set('secureDnsToggle_.value', true);
    this.updateDnsPrefs_(this.secureDnsRadio_);
  }
  //</if>

  /**
   * Updates the underlying secure DNS prefs based on the newly selected radio
   * button. This should only be called from the HTML. Focuses the custom text
   * field if the custom option has been selected.
   */
  private onRadioSelectionChanged_(event: CustomEvent<{value: SecureDnsMode}>) {
    if (event.detail.value === SecureDnsMode.SECURE &&
        !this.$.secureResolverSelect.value) {
      this.$.secureDnsInput.focus();
    }
    this.updateDnsPrefs_(event.detail.value);
  }

  /**
   * Helper method for updating the underlying secure DNS prefs based on the
   * provided mode and templates (if the latter is specified). The templates
   * param should only be specified when the underlying prefs are being updated
   * after a custom entry has been validated.
   */
  private updateDnsPrefs_(mode: SecureDnsMode, templates: string = '') {
    switch (mode) {
      case SecureDnsMode.SECURE:
        // If going to secure mode, set the templates pref first to prevent the
        // stub resolver config from being momentarily invalid. If the user has
        // selected the custom dropdown option, only update the underlying
        // prefs if the templates param was specified. If the templates param
        // was not specified, the custom entry may be invalid or may not
        // have passed validation yet, and we should not update either the
        // underlying mode or templates prefs.
        if (!this.$.secureResolverSelect.value) {
          if (!templates) {
            return;
          }
          this.setPrefValue('dns_over_https.templates', templates);
        } else {
          this.setPrefValue(
              'dns_over_https.templates', this.$.secureResolverSelect.value);
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
   * Prevent interactions with the dropdown menu or custom text field from
   * causing the corresponding radio button to be selected.
   */
  private stopEventPropagation_(event: Event) {
    event.stopPropagation();
  }

  /**
   * Updates the underlying secure DNS templates pref based on the selected
   * resolver and displays the corresponding privacy policy. Focuses the custom
   * text field if the custom option has been selected.
   */
  private onDropdownSelectionChanged_() {
    // If we're already in secure mode, update the prefs.
    if (this.secureDnsRadio_ === SecureDnsMode.SECURE) {
      this.updateDnsPrefs_(SecureDnsMode.SECURE);
    }
    this.updatePrivacyPolicyLine_();

    if (!this.$.secureResolverSelect.value) {
      this.$.secureDnsInput.focus();
    }
  }

  /**
   * Updates the setting to communicate the type of management, if any. The
   * setting is always collapsed if there is any management.
   */
  private updateManagementView_(setting: SecureDnsSetting) {
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
    let secureDescription = loadTimeData.getString('secureDnsDescription');
    // <if expr="chromeos_ash">
    if (this.isRevampWayfindingEnabled_) {
      secureDescription =
          loadTimeData.getString('secureDnsOsSettingsDescription');
    }

    if (setting.dohWithIdentifiersActive) {
      secureDescription = loadTimeData.substituteString(
          loadTimeData.getString('secureDnsWithIdentifiersDescription'),
          setting.configForDisplay);
    }
    // </if>

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
      this.showRadioGroup_ = false;
    } else {
      this.showRadioGroup_ = this.secureDnsToggle_.value;
    }
  }

  /**
   * Updates the UI to represent the given secure DNS config.
   * @param secureDnsConfig The current host resolver configuration.
   */
  private updateConfigRepresentation_(secureDnsConfig: string) {
    // If it is one of the non-custom dropdown options, select that option.
    const resolver =
        this.resolverOptions_.slice(1).find(r => r.value === secureDnsConfig);
    if (resolver) {
      this.$.secureResolverSelect.value = resolver.value;
      return;
    }

    // Otherwise, select the custom option.
    this.$.secureResolverSelect.value = '';

    // Only update the custom input field if the config string is non-empty.
    // Otherwise, we may be clearing a previous value that the user wishes to
    // reuse.
    if (secureDnsConfig.length > 0) {
      this.secureDnsInputValue_ = secureDnsConfig;
    }
  }

  /**
   * Displays the privacy policy corresponding to the selected dropdown resolver
   * or hides the privacy policy line if a custom resolver is selected.
   */
  private updatePrivacyPolicyLine_() {
    // If the selected item is the custom provider option, hide the privacy
    // policy line.
    if (!this.$.secureResolverSelect.value) {
      this.$.privacyPolicy.style.display = 'none';
      this.$.secureDnsInput.style.display = 'block';
      return;
    }

    // Otherwise, display the corresponding privacy policy.
    this.$.privacyPolicy.style.display = 'block';
    this.$.secureDnsInput.style.display = 'none';
    const resolver = this.resolverOptions_.find(
        r => r.value === this.$.secureResolverSelect.value);
    if (!resolver) {
      return;
    }

    this.privacyPolicyString_ = sanitizeInnerHtml(loadTimeData.substituteString(
        loadTimeData.getString('secureDnsSecureDropdownModePrivacyPolicy'),
        resolver.policy));
  }

  /**
   * Updates the underlying prefs if a custom entry was determined to be valid.
   * If the custom entry was determined to be invalid, moves the selected radio
   * button away from 'secure' if necessary.
   */
  private onSecureDnsInputEvaluated_(
      event: CustomEvent<{text: string, isValid: boolean}>) {
    if (event.detail.isValid) {
      this.updateDnsPrefs_(this.secureDnsRadio_, event.detail.text);
    }
  }

  // <if expr="chromeos_ash">
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
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-secure-dns': SettingsSecureDnsElement;
  }
}

customElements.define(SettingsSecureDnsElement.is, SettingsSecureDnsElement);
