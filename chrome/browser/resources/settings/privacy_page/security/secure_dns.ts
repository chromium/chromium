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

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import '../../controls/settings_toggle_button.js';
import './secure_dns_input.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import type {ResolverOption, SecureDnsSetting, SecurityPageBrowserProxy} from '/shared/settings/security_page/security_page_browser_proxy.js';
import {SecureDnsMode, SecureDnsUiManagementMode, SecurityPageBrowserProxyImpl} from '/shared/settings/security_page/security_page_browser_proxy.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';

import {getCss} from './secure_dns.css.js';
import {getHtml} from './secure_dns.html.js';
import type {SecureDnsInputElement} from './secure_dns_input.js';

export interface SettingsSecureDnsElement {
  $: {
    privacyPolicy: HTMLElement,
    secureDnsInput: SecureDnsInputElement,
    secureDnsInputContainer: HTMLElement,
    secureDnsToggle: SettingsToggleButtonElement,
    resolverSelect: HTMLSelectElement,
  };
}

const SettingsSecureDnsElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

/**
 * Enum for the categories of options in the secure DNS resolver select
 * menu.
 */
export enum SecureDnsResolverType {
  AUTOMATIC = 'automatic',
  BUILT_IN = 'built-in',
  CUSTOM = 'custom',
}

export type SecureDnsElement = SettingsSecureDnsElement;

export class SettingsSecureDnsElement extends SettingsSecureDnsElementBase {
  static get is() {
    return 'settings-secure-dns';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      secureDnsDescription_: {type: String},
      secureDnsToggle_: {type: Object},
      showSecureDnsOptions_: {type: Boolean},
      resolverOptions_: {type: Array},
      privacyPolicyString_: {type: String},
      secureDnsInputValue_: {type: String},
    };
  }

  protected accessor secureDnsDescription_: string = '';
  protected accessor secureDnsToggle_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: '',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };
  protected accessor showSecureDnsOptions_: boolean = false;
  protected accessor resolverOptions_: ResolverOption[] = [];
  protected accessor privacyPolicyString_: TrustedHTML =
      window.trustedTypes!.emptyHTML;
  protected accessor secureDnsInputValue_: string = '';

  private browserProxy_: SecurityPageBrowserProxy =
      SecurityPageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    // Fetch the options for the dropdown menu before configuring the setting
    // to match the underlying host resolver configuration.
    this.browserProxy_.getSecureDnsResolverList().then(resolvers => {
      this.resolverOptions_ = resolvers;
      this.browserProxy_.getSecureDnsSetting().then(
          async (setting: SecureDnsSetting) => {
            await PrefService.getInstance().whenInitialized();
            this.onSecureDnsPrefsChanged_(setting);
          });

      // Listen to changes in the host resolver configuration and update the
      // UI representation to match. (Changes to the host resolver configuration
      // may be generated in ways other than direct UI manipulation).
      this.addWebUiListener(
          'secure-dns-setting-changed',
          (setting: SecureDnsSetting) =>
              this.onSecureDnsPrefsChanged_(setting));
    });
  }

  /**
   * Update the UI representation to match the underlying host resolver
   * configuration.
   */
  private onSecureDnsPrefsChanged_(setting: SecureDnsSetting) {
    switch (setting.mode) {
      case SecureDnsMode.SECURE:
      case SecureDnsMode.AUTOMATIC:
        this.secureDnsToggle_ = {
          ...this.secureDnsToggle_,
          value: true,
        };
        this.updateConfigRepresentation_(setting.mode, setting.config);
        break;
      case SecureDnsMode.OFF:
        this.secureDnsToggle_ = {
          ...this.secureDnsToggle_,
          value: false,
        };
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }

    this.updateManagementView_(setting);
  }

  /**
   * Updates the underlying secure DNS mode pref based on the new toggle
   * selection (and the underlying select menu if the toggle has just been
   * turned on).
   */
  protected async onToggleChange_() {
    this.showSecureDnsOptions_ = this.$.secureDnsToggle.checked;
    this.secureDnsToggle_ = {
      ...this.secureDnsToggle_,
      value: this.$.secureDnsToggle.checked,
    };

    if (!this.secureDnsToggle_.value) {
      this.updateDnsPrefs_(SecureDnsMode.OFF);
      return;
    }

    const resolver = this.$.resolverSelect.value;
    if (resolver === SecureDnsResolverType.AUTOMATIC) {
      this.updateDnsPrefs_(SecureDnsMode.AUTOMATIC);
    } else {
      if (resolver === SecureDnsResolverType.CUSTOM) {
        await this.updateComplete;
        this.$.secureDnsInput.focus();
      }
      this.updateDnsPrefs_(SecureDnsMode.SECURE);
    }
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
        const builtInResolver = this.builtInResolver_();
        if (!builtInResolver) {
          if (!templates) {
            return;
          }
          PrefService.getInstance().setPrefValue(
              'dns_over_https.templates', templates);
        } else {
          PrefService.getInstance().setPrefValue(
              'dns_over_https.templates', builtInResolver.value);
        }
        PrefService.getInstance().setPrefValue('dns_over_https.mode', mode);
        break;
      case SecureDnsMode.AUTOMATIC:
      case SecureDnsMode.OFF:
        // If going to automatic or off mode, set the mode pref first to avoid
        // clearing the dropdown selection when the templates pref is cleared.
        PrefService.getInstance().setPrefValue('dns_over_https.mode', mode);
        PrefService.getInstance().setPrefValue('dns_over_https.templates', '');
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }
  }

  /**
   * Updates the underlying secure DNS templates pref based on the selected
   * resolver and displays the corresponding privacy policy.
   */
  protected onDropdownSelectionChange_() {
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
  private updateManagementView_(setting: SecureDnsSetting) {
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
    const secureDescription = loadTimeData.getString('secureDnsDescription');
    const modePref =
        PrefService.getInstance().getPref<SecureDnsMode>('dns_over_https.mode');

    if (modePref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = modePref.controlledBy;
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
  private updateConfigRepresentation_(mode: SecureDnsMode, template: string) {
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
        break;
      case SecureDnsMode.SECURE:
        if (index === -1) {
          selectValue = SecureDnsResolverType.CUSTOM;
          hideCustomEntry = false;
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
      if (template) {
        this.secureDnsInputValue_ = template;
      } else {
        this.secureDnsInputValue_ = this.$.secureDnsInput.value;
        this.$.secureDnsInput.focus();
      }
    }
  }

  /**
   * Displays the privacy policy string if the policy URL is specified,
   * otherwise hides it.
   * @param policy The privacy policy URL.
   */
  private updatePrivacyPolicyLine_(policy: string) {
    // If the selected item is the custom resolver option, hide the privacy
    // policy line.
    if (!policy) {
      this.$.privacyPolicy.style.display = 'none';
      return;
    }

    // Otherwise, display the corresponding privacy policy.
    this.$.privacyPolicy.style.display = 'block';

    this.privacyPolicyString_ = sanitizeInnerHtml(loadTimeData.substituteString(
        loadTimeData.getString('secureDnsSecureDropdownModePrivacyPolicy'),
        policy));
  }

  /**
   * Updates the underlying prefs if a custom entry was determined to be valid.
   */
  protected onSecureDnsInputValueUpdate_(
      event: CustomEvent<{text: string, isValid: boolean}>) {
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-secure-dns': SettingsSecureDnsElement;
  }
}

customElements.define(SettingsSecureDnsElement.is, SettingsSecureDnsElement);
