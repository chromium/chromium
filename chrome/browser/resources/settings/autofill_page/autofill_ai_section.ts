// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-ai-section' contains configuration options
 * for Autofill AI.
 */

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/prefs/prefs.js';
import '../ai_page/ai_logging_info_bullet.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';
// </if>

import './autofill_ai_entries_list.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName, ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './autofill_ai_section.html.js';
import type {EntityDataManagerProxy} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

export interface SettingsAutofillAiSectionElement {
  $: {
    prefToggle: SettingsToggleButtonElement,
  };
}

const SettingsAutofillAiSectionElementBase =
    SettingsViewMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsAutofillAiSectionElement extends
    SettingsAutofillAiSectionElementBase {
  static get is() {
    return 'settings-autofill-ai-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
         If a user is not eligible for Autofill with Ai, but they have data
         saved, the code allows them only to edit and delete their data. They
         are not allowed to add new data, or to opt-in or opt-out of Autofill
         with Ai using the toggle at the top of this page.
         If a user is not eligible for Autofill with Ai and they also have no
         data saved, then they cannot access this page at all.
       */
      ineligibleUser: {
        type: Boolean,
        value() {
          return !loadTimeData.getBoolean('userEligibleForAutofillAi');
        },
      },

      /**
         A "fake" preference object that reflects the state of the opt-in
         toggle and the presence/absence of an enterprise policy.
         This allows leveraging the settings-toggle-button component
         to reflect enterprise enabled/disabled states.
       */
      optedIn_: {
        type: Object,
        value: () => ({
          // Does not correspond to an actual pref - this is faked to allow
          // writing it into a GAIA-id keyed dictionary of opt-ins.
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        }),
      },

      /**
        If reflects whether Wallet server data is available for storage.
      */
      isWalletServerStorageEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isWalletServerStorageEnabled');
        },
      },
      /**
        If true, Autofill AI does not depend on whether Autofill for addresses
        is enabled.
      */
      autofillAiIgnoresWhetherAddressFillingIsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'AutofillAiIgnoresWhetherAddressFillingIsEnabled');
        },
      },
    };
  }

  static get observers() {
    return [
      `onAutofillAddressPrefChanged_(
          prefs.autofill.profile_enabled.value)`,
    ];
  }

  declare ineligibleUser: boolean;
  declare private optedIn_: chrome.settingsPrivate.PrefObject;
  declare private isWalletServerStorageEnabled_: boolean;
  declare private autofillAiIgnoresWhetherAddressFillingIsEnabled_: boolean;

  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.entityDataManager_.getOptInStatus().then(
        optedIn => this.set('optedIn_.value', !this.ineligibleUser && optedIn));
    const policyDisabled =
        this.getPref(AiEnterpriseFeaturePrefName.AUTOFILL_AI).value ===
        ModelExecutionEnterprisePolicyValue.DISABLE;
    if (policyDisabled) {
      this.set(
          'optedIn_.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
      this.set(
          'optedIn_.controlledBy',
          chrome.settingsPrivate.ControlledBy.USER_POLICY);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  private async onOptInToggleChange_() {
    // `setOptInStatus` returns false when the user tries to toggle the opt-in
    // status when they're ineligible.  This shouldn't happen usually but in
    // some cases it can happen (see crbug.com/408145195).
    this.ineligibleUser = !(await this.entityDataManager_.setOptInStatus(
        this.$.prefToggle.checked));
    if (this.ineligibleUser) {
      this.set('optedIn_.value', false);
    }
  }

  /**
   * Whether an info bullet regarding logging is shown. Autofill Ai only shows
   * logging behaviour information for enterprise clients who have either the
   * feature disabled or just logging disabled.
   */
  private showLoggingInfoBullet_(pref: number) {
    return pref !== ModelExecutionEnterprisePolicyValue.ALLOW;
  }

  // Adjusts the opt-in state when address autofill status changes.
  //
  // This covers the case where a user disables address autofill and then checks
  // the AutofillAI opt-in status. In this case, we do not remove the AutofillAI
  // entry, but just set the opt-in to false. Note that other
  // preconditions (e.g., sync) are not covered.
  private async onAutofillAddressPrefChanged_(prefValue: boolean) {
    if (this.autofillAiIgnoresWhetherAddressFillingIsEnabled_) {
      return;
    }
    const optedIn = await this.entityDataManager_.getOptInStatus();
    this.set('optedIn_.value', !this.ineligibleUser && optedIn && prefValue);
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-ai-section': SettingsAutofillAiSectionElement;
  }
}

customElements.define(
    SettingsAutofillAiSectionElement.is, SettingsAutofillAiSectionElement);
