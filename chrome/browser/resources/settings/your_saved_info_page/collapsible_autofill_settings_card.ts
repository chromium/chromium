// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'collapsible-card' is a container component used to display Autofill-related
 * settings. It can be expanded or collapsed by the user.
 */

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/prefs/prefs.js';
import '../ai_page/ai_logging_info_bullet.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName, ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from '../autofill_page/entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../autofill_page/entity_data_manager_proxy.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './collapsible_autofill_settings_card.html.js';

export interface CollapsibleCardElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

export class CollapsibleCardElement extends
    SettingsViewMixin(PrefsMixin(PolymerElement)) {
  static get is() {
    return 'collapsible-autofill-settings-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Controls the expanded/collapsed state of the details.
       */
      expanded_: {type: Boolean, value: false},

      /**
         Indicates if a user is eligible to change Enhanced Autofill data.
         If a user is not eligible for Enhanced Autofill (Autofill with Ai),
         but they have data saved, the code allows them only to edit and delete
         their data. They are not allowed to add new data, or to opt-in or
         opt-out of Enhanced Autofill using the corresponding toggle in this
         component. If a user is not eligible for Enhanced Autofill and they
         also have no data saved, then they cannot access this page at all.
       */
      enhancedAutofillEligibleUser_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('userEligibleForAutofillAi');
        },
      },

      /**
         A "fake" preference object that reflects the state of the opt-in
         toggle for Enhanced Autofill and the presence/absence of an enterprise
         policy. This allows leveraging the settings-toggle-button component
         to reflect enterprise enabled/disabled states.
       */
      enhancedAutofillOptedIn_: {
        type: Object,
        value: () => ({
          // Does not correspond to an actual pref - this is done to allow
          // writing it into a GAIA-id keyed dictionary of opt-ins.
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        }),
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
      'onAutofillAddressPrefChanged_(prefs.autofill.profile_enabled.value)',
      `onEnterprisePolicyChanged_(prefs.${
          AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value)`,
    ];
  }

  declare private expanded_: boolean;
  declare private enhancedAutofillEligibleUser_: boolean;
  declare private enhancedAutofillOptedIn_: chrome.settingsPrivate.PrefObject;
  declare private autofillAiIgnoresWhetherAddressFillingIsEnabled_: boolean;

  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.entityDataManager_.getOptInStatus().then(enhancedAutofillOptedIn => {
      this.set(
          'enhancedAutofillOptedIn_.value',
          this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.entityInstancesChangedListener_) {
      this.entityDataManager_.removeEntityInstancesChangedListener(
          this.entityInstancesChangedListener_);
      this.entityInstancesChangedListener_ = null;
    }
  }

  private async onOptInToggleChange_() {
    // `setOptInStatus` returns false when the user tries to toggle the opt-in
    // status when they're ineligible.  This shouldn't happen usually but in
    // some cases it can happen (see crbug.com/408145195).
    this.enhancedAutofillEligibleUser_ =
        (await this.entityDataManager_.setOptInStatus(
            this.$.optInToggle.checked));
    if (!this.enhancedAutofillEligibleUser_) {
      this.set('enhancedAutofillOptedIn_.value', false);
    }
  }

  /**
   * Whether an info bullet regarding logging is shown. Enhanced Autofill only
   * shows logging behaviour information for enterprise clients who have either
   * the feature disabled or just logging disabled.
   */
  private showLoggingInfoBullet_(prefValue: number): boolean {
    return prefValue !== ModelExecutionEnterprisePolicyValue.ALLOW;
  }

  // Adjusts the opt-in state when address autofill status changes.
  // This covers the case where a user disables address autofill and then checks
  // the AutofillAI opt-in status. In this case, we do not remove the AutofillAI
  // entry, but just set the opt-in to false. Note that other
  // preconditions (e.g., sync) are not covered.
  private async onAutofillAddressPrefChanged_(prefValue: boolean) {
    if (this.autofillAiIgnoresWhetherAddressFillingIsEnabled_) {
      return;
    }
    const enhancedAutofillOptedIn =
        await this.entityDataManager_.getOptInStatus();
    this.set(
        'enhancedAutofillOptedIn_.value',
        this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn &&
            prefValue);
  }

  /**
   * Observes changes to the enterprise policy for Autofill AI keeping the
   * component's state up to date. When the policy disables the feature, updates
   * the UI to reflect the enforced state, disabling the toggle. When the policy
   * is lifted, it asynchronously fetches the user's latest opt-in status to
   * accurately restore the toggle's state without blocking the UI.
   */
  private async onEnterprisePolicyChanged_(
      policyValue: ModelExecutionEnterprisePolicyValue|undefined) {
    if (policyValue === undefined) {
      return;
    }

    if (policyValue === ModelExecutionEnterprisePolicyValue.DISABLE) {
      this.set(
          'enhancedAutofillOptedIn_.enforcement',
          chrome.settingsPrivate.Enforcement.ENFORCED);
      this.set(
          'enhancedAutofillOptedIn_.controlledBy',
          chrome.settingsPrivate.ControlledBy.USER_POLICY);
      this.set('enhancedAutofillOptedIn_.value', false);
    } else {
      this.set('enhancedAutofillOptedIn_.enforcement', undefined);
      this.set('enhancedAutofillOptedIn_.controlledBy', undefined);

      const enhancedAutofillOptedIn =
          await this.entityDataManager_.getOptInStatus();
      const autofillEnabled = this.get('prefs.autofill.profile_enabled.value');
      this.set(
          'enhancedAutofillOptedIn_.value',
          this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn &&
              autofillEnabled);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'collapsible-autofill-settings-card': CollapsibleCardElement;
  }
}

customElements.define(CollapsibleCardElement.is, CollapsibleCardElement);
