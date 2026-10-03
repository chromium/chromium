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
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../ai_page/ai_logging_info_bullet.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';
import './walletable_pass_detection_toggle.js';

// </if>

import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName, ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './collapsible_autofill_settings_card.html.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

export interface CollapsibleCardElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

export class CollapsibleCardElement extends SettingsViewMixin
(PrefServiceObserverMixin(I18nMixin(PolymerElement))) {
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
         their data. They are not allowed to add new data, or to opt in or
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
       * Indicates whether the feature `kAutofillAiReauthRequired` is enabled.
       */
      // <if expr="is_win or is_macosx or is_chromeos">
      autofillAiReauthOnViewingSensitiveDataEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'autofillAiReauthOnViewingSensitiveDataEnabled');
        },
      },
      // </if>
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

      isUserEligibleForWalletablePassDetection_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isUserEligibleForWalletablePassDetection');
        },
      },

      /**
       * If true, Autofill AI does not depend on whether Autofill for addresses
       * is enabled.
       */
      autofillSettingsEnterprisePolicyEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'AutofillSettingsEnterprisePolicyEnabled');
        },
      },

      profileEnabledPref_: {
        type: Object,
        value: null,
      },

      autofillAiEnterprisePolicyPref_: {
        type: Object,
        value: null,
      },
    };
  }

  static get observers() {
    return [
      'onEnterprisePolicyChanged_(autofillAiEnterprisePolicyPref_, profileEnabledPref_)',
    ];
  }

  declare private expanded_: boolean;
  declare private enhancedAutofillEligibleUser_: boolean;
  // <if expr="is_win or is_macosx or is_chromeos">
  declare private autofillAiReauthOnViewingSensitiveDataEnabled_: boolean;
  // </if>
  declare private enhancedAutofillOptedIn_: chrome.settingsPrivate.PrefObject;
  declare private isUserEligibleForWalletablePassDetection_: boolean;
  declare private autofillSettingsEnterprisePolicyEnabled_: boolean;
  declare private profileEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|null;
  declare private autofillAiEnterprisePolicyPref_:
      chrome.settingsPrivate.PrefObject<ModelExecutionEnterprisePolicyValue>|
      null;

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

  private onChangeAuthenticationRequirementClicked_(e: Event) {
    e.preventDefault();
    if (!this.enhancedAutofillEligibleUser_) {
      return;
    }
    this.entityDataManager_.toggleAutofillAiReauthRequirement();
  }

  /**
   * Whether an info bullet regarding logging is shown. Enhanced Autofill only
   * shows logging behaviour information for enterprise clients who have either
   * the feature disabled or just logging disabled.
   */
  private showLoggingInfoBullet_(prefValue: number): boolean {
    return prefValue !== ModelExecutionEnterprisePolicyValue.ALLOW;
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
      this.set(
          'enhancedAutofillOptedIn_.enforcement',
          addressAutofillEnabled.enforcement);
      this.set(
          'enhancedAutofillOptedIn_.controlledBy',
          addressAutofillEnabled.controlledBy);
      // We need to check addressAutofillEnabled.value here.
      // this.enhancedAutofillEligibleUser_ does consider
      // addressAutofillEnabled.value, but loadTimeData constants are
      // refreshed only after page reload.
      this.set(
          'enhancedAutofillOptedIn_.value',
          this.enhancedAutofillEligibleUser_ && addressAutofillEnabled.value);
      return;
    }

    const autofillAiPolicyValue = this.autofillAiEnterprisePolicyPref_.value;

    if (autofillAiPolicyValue === ModelExecutionEnterprisePolicyValue.DISABLE) {
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

      if (this.autofillSettingsEnterprisePolicyEnabled_) {
        this.set(
            'enhancedAutofillOptedIn_.value',
            this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn);
      } else {
        this.set(
            'enhancedAutofillOptedIn_.value',
            this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn &&
                addressAutofillEnabled.value);
      }
    }
  }

  private showExtensionControlledIndicator_(): boolean {
    if (!this.profileEnabledPref_) {
      return false;
    }

    return !!this.profileEnabledPref_.extensionId &&
        !this.profileEnabledPref_.value;
  }

  private optInToggleDisabled_(): boolean {
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
    'collapsible-autofill-settings-card': CollapsibleCardElement;
  }
}

customElements.define(CollapsibleCardElement.is, CollapsibleCardElement);
