// Copyright 2020 The Chromium Authors. All rights reserved.
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

import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';
import './secure_dns_input.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';

import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl, ResolverOption, SecureDnsMode, SecureDnsSetting, SecureDnsUiManagementMode} from './privacy_page_browser_proxy.js';

Polymer({
  is: 'settings-secure-dns',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Mirroring the secure DNS mode enum so that it can be used from HTML
     * bindings.
     * @private {!SecureDnsMode}
     */
    secureDnsModeEnum_: {
      type: Object,
      value: SecureDnsMode,
    },

    /**
     * The setting sublabel.
     * @private
     */
    secureDnsDescription_: String,

    /**
     * Represents whether the main toggle for the secure DNS setting is switched
     * on or off.
     * @private {!chrome.settingsPrivate.PrefObject}
     */
    secureDnsToggle_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        });
      },
    },

    /**
     * Whether the radio buttons should be shown.
     * @private
     */
    showRadioGroup_: Boolean,

    /**
     * Represents the selected radio button. Should always have a value of
     * 'automatic' or 'secure'.
     * @private {!SecureDnsMode}
     */
    secureDnsRadio_: {
      type: String,
      value: SecureDnsMode.AUTOMATIC,
    },

    /**
     * List of secure DNS resolvers to display in dropdown menu.
     * @private {!Array<!ResolverOption>}
     */
    resolverOptions_: Array,

    /**
     * Track the selected dropdown option so that it can be logged when a user-
     * initiated UI dropdown selection change event occurs.
     * @private
     */
    lastResolverOption_: String,

    /**
     * String displaying the privacy policy of the resolver selected in the
     * dropdown menu.
     * @private
     */
    privacyPolicyString_: String,

    /**
     * String to display in the custom text field.
     * @private
     */
    secureDnsInputValue_: String,
  },

  /** @private {?PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = PrivacyPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    // Fetch the options for the dropdown menu before configuring the setting
    // to match the underlying host resolver configuration.
    this.browserProxy_.getSecureDnsResolverList().then(resolvers => {
      this.resolverOptions_ = resolvers;
      this.lastResolverOption_ = this.resolverOptions_[0].value;
      this.browserProxy_.getSecureDnsSetting().then(
          this.onSecureDnsPrefsChanged_.bind(this));

      // Listen to changes in the host resolver configuration and update the
      // UI representation to match. (Changes to the host resolver configuration
      // may be generated in ways other than direct UI manipulation).
      this.addWebUIListener(
          'secure-dns-setting-changed',
          this.onSecureDnsPrefsChanged_.bind(this));
    });
  },

  /**
   * Update the UI representation to match the underlying host resolver
   * configuration.
   * @param {!SecureDnsSetting} setting
   * @private
   */
  onSecureDnsPrefsChanged_: function(setting) {
    switch (setting.mode) {
      case SecureDnsMode.SECURE:
        this.set('secureDnsToggle_.value', true);
        this.secureDnsRadio_ = SecureDnsMode.SECURE;
        // Only update the selected dropdown item if the user is in secure
        // mode. Otherwise, we may be losing a selection that hasn't been
        // pushed yet to prefs.
        this.updateTemplatesRepresentation_(setting.templates);
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

    this.updateManagementView_(setting.managementMode);
  },

  /**
   * Updates the underlying secure DNS mode pref based on the new toggle
   * selection (and the underlying radio button if the toggle has just been
   * enabled).
   * @private
   */
  onToggleChanged_: function() {
    this.showRadioGroup_ =
        /** @type {boolean} */ (this.secureDnsToggle_.value);
    if (this.secureDnsRadio_ === SecureDnsMode.SECURE &&
        !this.$.secureResolverSelect.value) {
      this.$.secureDnsInput.focus();
    }
    this.updateDnsPrefs_(
        this.secureDnsToggle_.value ? this.secureDnsRadio_ : SecureDnsMode.OFF);
  },

  /**
   * Updates the underlying secure DNS prefs based on the newly selected radio
   * button. This should only be called from the HTML. Focuses the custom text
   * field if the custom option has been selected.
   * @param {!CustomEvent<{value: !SecureDnsMode}>} event
   * @private
   */
  onRadioSelectionChanged_: function(event) {
    if (event.detail.value === SecureDnsMode.SECURE &&
        !this.$.secureResolverSelect.value) {
      this.$.secureDnsInput.focus();
    }
    this.updateDnsPrefs_(event.detail.value);
  },

  /**
   * Helper method for updating the underlying secure DNS prefs based on the
   * provided mode and templates (if the latter is specified). The templates
   * param should only be specified when the underlying prefs are being updated
   * after a custom entry has been validated.
   * @param {!SecureDnsMode} mode
   * @param {string=} templates
   * @private
   */
  updateDnsPrefs_: function(mode, templates = '') {
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
  },

  /**
   * Prevent interactions with the dropdown menu or custom text field from
   * causing the corresponding radio button to be selected.
   * @param {!Event} event
   * @private
   */
  stopEventPropagation_: function(event) {
    event.stopPropagation();
  },

  /**
   * Updates the underlying secure DNS templates pref based on the selected
   * resolver and displays the corresponding privacy policy. Focuses the custom
   * text field if the custom option has been selected.
   * @private
   */
  onDropdownSelectionChanged_: function() {
    // If we're already in secure mode, update the prefs.
    if (this.secureDnsRadio_ === SecureDnsMode.SECURE) {
      this.updateDnsPrefs_(SecureDnsMode.SECURE);
    }
    this.updatePrivacyPolicyLine_();

    if (!this.$.secureResolverSelect.value) {
      this.$.secureDnsInput.focus();
    }

    this.browserProxy_.recordUserDropdownInteraction(
        this.lastResolverOption_, this.$.secureResolverSelect.value);
    this.lastResolverOption_ = this.$.secureResolverSelect.value;
  },

  /**
   * Updates the setting to communicate the type of management, if any. The
   * setting is always collapsed if there is any management.
   * @param {!SecureDnsUiManagementMode} managementMode
   * @private
   */
  updateManagementView_: function(managementMode) {
    if (this.prefs === undefined) {
      return;
    }
    // If the underlying secure DNS mode pref has an enforced value, communicate
    // that via the toggle pref.
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: this.secureDnsToggle_.value,
    };
    if (this.getPref('dns_over_https.mode').enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = this.getPref('dns_over_https.mode').controlledBy;
      this.secureDnsDescription_ =
          loadTimeData.getString('secureDnsDescription');
    } else {
      // If the secure DNS mode was forcefully overridden by Chrome, provide an
      // explanation in the setting subtitle.
      switch (managementMode) {
        case SecureDnsUiManagementMode.NO_OVERRIDE:
          this.secureDnsDescription_ =
              loadTimeData.getString('secureDnsDescription');
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
              'Received unknown secure DNS management mode ' + managementMode);
      }
    }
    this.secureDnsToggle_ = pref;

    if (this.secureDnsToggle_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      this.showRadioGroup_ = false;
    } else {
      this.showRadioGroup_ =
          /** @type {boolean} */ (this.secureDnsToggle_.value);
    }
  },

  /**
   * Updates the UI to represent the given secure DNS templates.
   * @param {Array<string>} secureDnsTemplates List of secure DNS templates in
   *     the current host resolver configuration.
   * @private
   */
  updateTemplatesRepresentation_: function(secureDnsTemplates) {
    // If there is exactly one template and it is one of the non-custom dropdown
    // options, select that option.
    if (secureDnsTemplates.length === 1) {
      const resolver = this.resolverOptions_.slice(1).find(
          r => r.value === secureDnsTemplates[0]);
      if (resolver) {
        this.$.secureResolverSelect.value = resolver.value;
        this.lastResolverOption_ = resolver.value;
        return;
      }
    }

    // Otherwise, select the custom option.
    this.$.secureResolverSelect.value = '';
    this.lastResolverOption_ = '';

    // Only update the custom input field if the templates are non-empty.
    // Otherwise, we may be clearing a previous value that the user wishes to
    // reuse.
    if (secureDnsTemplates.length > 0) {
      this.secureDnsInputValue_ = secureDnsTemplates.join(' ');
    }
  },

  /**
   * Displays the privacy policy corresponding to the selected dropdown resolver
   * or hides the privacy policy line if a custom resolver is selected.
   * @private
   */
  updatePrivacyPolicyLine_: function() {
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

    this.privacyPolicyString_ = loadTimeData.substituteString(
        loadTimeData.getString('secureDnsSecureDropdownModePrivacyPolicy'),
        resolver.policy);
  },

  /**
   * Updates the underlying prefs if a custom entry was determined to be valid.
   * If the custom entry was determined to be invalid, moves the selected radio
   * button away from 'secure' if necessary.
   * @param {!CustomEvent<!{text: string, isValid: boolean}>} event
   * @private
   */
  onSecureDnsInputEvaluated_: function(event) {
    if (event.detail.isValid) {
      this.updateDnsPrefs_(this.secureDnsRadio_, event.detail.text);
    }
  },
});
