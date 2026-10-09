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
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../ai_page/ai_logging_info_bullet.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';
import './walletable_pass_detection_toggle.js';

// </if>

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AiEnterpriseFeaturePrefName, ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './collapsible_autofill_settings_card.css.js';
import {getHtml} from './collapsible_autofill_settings_card.html.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

export interface CollapsibleAutofillSettingsCardElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

const CollapsibleAutofillSettingsCardElementBase =
    PrefServiceObserverMixinLit(CrLitElement);

export class CollapsibleAutofillSettingsCardElement extends
    CollapsibleAutofillSettingsCardElementBase {
  static get is() {
    return 'collapsible-autofill-settings-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Controls the expanded/collapsed state of the details.
       */
      expanded_: {type: Boolean},

      /**
         Indicates if a user is eligible to change Enhanced Autofill data.
         If a user is not eligible for Enhanced Autofill (Autofill with Ai),
         but they have data saved, the code allows them only to edit and delete
         their data. They are not allowed to add new data, or to opt in or
         opt-out of Enhanced Autofill using the corresponding toggle in this
         component. If a user is not eligible for Enhanced Autofill and they
         also have no data saved, then they cannot access this page at all.
       */
      enhancedAutofillEligibleUser_: {type: Boolean},

      /**
       * Indicates whether the feature `kAutofillAiReauthRequired` is enabled.
       */
      // <if expr="is_win or is_macosx or is_chromeos">
      autofillAiReauthOnViewingSensitiveDataEnabled_: {type: Boolean},
      // </if>

      /**
         A "fake" preference object that reflects the state of the opt-in
         toggle for Enhanced Autofill and the presence/absence of an enterprise
         policy. This allows leveraging the settings-toggle-button component
         to reflect enterprise enabled/disabled states.
       */
      enhancedAutofillOptedIn_: {type: Object},

      isUserEligibleForWalletablePassDetection_: {type: Boolean},

      /**
       * If true, Autofill AI does not depend on whether Autofill for addresses
       * is enabled.
       */
      autofillSettingsEnterprisePolicyEnabled_: {type: Boolean},

      profileEnabledPref_: {type: Object},

      autofillAiEnterprisePolicyPref_: {type: Object},
    };
  }

  protected accessor expanded_: boolean = false;
  protected accessor enhancedAutofillEligibleUser_: boolean =
      loadTimeData.getBoolean('userEligibleForAutofillAi');
  // <if expr="is_win or is_macosx or is_chromeos">
  protected accessor autofillAiReauthOnViewingSensitiveDataEnabled_: boolean =
      loadTimeData.getBoolean('autofillAiReauthOnViewingSensitiveDataEnabled');
  // </if>
  protected accessor enhancedAutofillOptedIn_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    // Does not correspond to an actual pref - this is done to allow
    // writing it into a GAIA-id keyed dictionary of opt-ins.
    key: '',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };
  protected accessor isUserEligibleForWalletablePassDetection_: boolean =
      loadTimeData.getBoolean('isUserEligibleForWalletablePassDetection');
  protected accessor autofillSettingsEnterprisePolicyEnabled_: boolean =
      loadTimeData.getBoolean('AutofillSettingsEnterprisePolicyEnabled');
  protected accessor profileEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor autofillAiEnterprisePolicyPref_:
      chrome.settingsPrivate.PrefObject<ModelExecutionEnterprisePolicyValue>|
      undefined;

  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'autofill.profile_enabled': 'profileEnabledPref_',
      [AiEnterpriseFeaturePrefName.AUTOFILL_AI]:
          'autofillAiEnterprisePolicyPref_',
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('autofillAiEnterprisePolicyPref_') ||
        changedPrivateProperties.has('profileEnabledPref_')) {
      this.onEnterprisePolicyChanged_();
    }
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected async onOptInToggleSettingsBooleanControlChange_() {
    // `setOptInStatus` returns false when the user tries to toggle the opt-in
    // status when they're ineligible.  This shouldn't happen usually but in
    // some cases it can happen (see crbug.com/408145195).
    this.enhancedAutofillEligibleUser_ =
        (await this.entityDataManager_.setOptInStatus(
            this.$.optInToggle.checked));
    if (!this.enhancedAutofillEligibleUser_) {
      this.enhancedAutofillOptedIn_ = {
        ...this.enhancedAutofillOptedIn_,
        value: false,
      };
    }
  }

  // <if expr="is_win or is_macosx or is_chromeos">
  protected onChangeAuthenticationRequirementClick_(e: Event) {
    e.preventDefault();
    if (!this.enhancedAutofillEligibleUser_) {
      return;
    }
    this.entityDataManager_.toggleAutofillAiReauthRequirement();
  }

  protected onChangeAuthenticationRequirementChange_(e: Event) {
    this.onChangeAuthenticationRequirementClick_(e);
  }
  // </if>

  /**
   * Whether an info bullet regarding logging is shown. Enhanced Autofill only
   * shows logging behaviour information for enterprise clients who have either
   * the feature disabled or just logging disabled.
   */
  protected showLoggingInfoBullet_(): boolean {
    return !!this.autofillAiEnterprisePolicyPref_ &&
        this.autofillAiEnterprisePolicyPref_.value !==
        ModelExecutionEnterprisePolicyValue.ALLOW;
  }

  /**
   * Observes changes to the enterprise policies for Address autofill and
   * Autofill AI keeping the component's state up to date. When the policy
   * disables the feature, updates the UI to reflect the enforced state,
   * disabling the toggle. When the policy is lifted, it asynchronously fetches
   * the user's latest opt-in status to accurately restore the toggle's state
   * without blocking the UI.
   */
  private async onEnterprisePolicyChanged_() {
    if (!this.profileEnabledPref_ || !this.autofillAiEnterprisePolicyPref_) {
      return;
    }
    const addressAutofillEnabled = this.profileEnabledPref_;

    if (!this.autofillSettingsEnterprisePolicyEnabled_ &&
        addressAutofillEnabled.enforcement ===
            chrome.settingsPrivate.Enforcement.ENFORCED &&
        !addressAutofillEnabled.value) {
      this.enhancedAutofillOptedIn_ = {
        ...this.enhancedAutofillOptedIn_,
        enforcement: addressAutofillEnabled.enforcement,
        controlledBy: addressAutofillEnabled.controlledBy,
        // We need to check addressAutofillEnabled.value here.
        // this.enhancedAutofillEligibleUser_ does consider
        // addressAutofillEnabled.value, but loadTimeData constants are
        // refreshed only after page reload.
        value:
            this.enhancedAutofillEligibleUser_ && addressAutofillEnabled.value,
      };
      return;
    }

    const autofillAiPolicyValue = this.autofillAiEnterprisePolicyPref_.value;

    if (autofillAiPolicyValue === ModelExecutionEnterprisePolicyValue.DISABLE) {
      this.enhancedAutofillOptedIn_ = {
        ...this.enhancedAutofillOptedIn_,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        value: false,
      };
    } else {
      this.enhancedAutofillOptedIn_ = {
        ...this.enhancedAutofillOptedIn_,
        enforcement: undefined,
        controlledBy: undefined,
      };

      const enhancedAutofillOptedIn =
          await this.entityDataManager_.getOptInStatus();

      this.enhancedAutofillOptedIn_ = {
        ...this.enhancedAutofillOptedIn_,
        value: this.autofillSettingsEnterprisePolicyEnabled_ ?
            this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn :
            this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn &&
                addressAutofillEnabled.value,
      };
    }
  }

  protected showExtensionControlledIndicator_(): boolean {
    if (!this.profileEnabledPref_) {
      return false;
    }

    return !!this.profileEnabledPref_.extensionId &&
        !this.profileEnabledPref_.value;
  }

  protected optInToggleDisabled_(): boolean {
    if (!this.profileEnabledPref_) {
      return true;
    }

    const addressAutofillEnforcedFalse =
        this.profileEnabledPref_.enforcement ===
            chrome.settingsPrivate.Enforcement.ENFORCED &&
        !this.profileEnabledPref_.value;
    // We need to check this.profileEnabledPref_.value here.
    // this.enhancedAutofillEligibleUser_ does consider
    // this.profileEnabledPref_.value, but loadTimeData constants are refreshed
    // only after page reload.
    return !this.enhancedAutofillEligibleUser_ || addressAutofillEnforcedFalse;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'collapsible-autofill-settings-card':
        CollapsibleAutofillSettingsCardElement;
  }
}

customElements.define(
    CollapsibleAutofillSettingsCardElement.is,
    CollapsibleAutofillSettingsCardElement);
