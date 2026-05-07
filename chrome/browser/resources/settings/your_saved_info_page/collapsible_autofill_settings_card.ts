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
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/prefs/prefs.js';
import '../ai_page/ai_logging_info_bullet.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';
import '../autofill_page/walletable_pass_detection_toggle.js';

// </if>

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName, ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from '../autofill_page/entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../autofill_page/entity_data_manager_proxy.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './collapsible_autofill_settings_card.html.js';

export interface CollapsibleCardElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

export class CollapsibleCardElement extends SettingsViewMixin
(PrefsMixin(I18nMixin(PolymerElement))) {
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
        If true, Autofill AI does not depend on whether Autofill for addresses
        is enabled.
      */
      autofillAddOtherDatatypesPrefIsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'AutofillAddOtherDatatypesPrefIsEnabled');
        },
      },

      /**
         Whether the feature kAutofillAiAvailableByDefault is enabled. When
         enabled, users do not need to opt-in to enhanced Autofill to use
         Autofill AI.
       */
      autofillAiAvailableByDefault_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('autofillAiAvailableByDefault');
        },
      },

      enableYourSavedInfoPolicyAndExtentionToggleIndicators_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableYourSavedInfoPolicyAndExtentionToggleIndicators');
        },
      },

      showAccessibilityAnnotatorSettingsLink_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'showAccessibilityAnnotatorSettingsLink');
        },
      },
    };
  }

  static get observers() {
    return [
      `onEnterprisePolicyChanged_(prefs.${
          AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value,
          prefs.autofill.profile_enabled.*)`,
    ];
  }

  declare private expanded_: boolean;
  declare private enhancedAutofillEligibleUser_: boolean;
  // <if expr="is_win or is_macosx or is_chromeos">
  declare private autofillAiReauthOnViewingSensitiveDataEnabled_: boolean;
  // </if>
  declare private enhancedAutofillOptedIn_: chrome.settingsPrivate.PrefObject;
  declare private isUserEligibleForWalletablePassDetection_: boolean;
  declare private autofillAddOtherDatatypesPrefIsEnabled_: boolean;
  declare private autofillAiAvailableByDefault_: boolean;
  declare private enableYourSavedInfoPolicyAndExtentionToggleIndicators_:
      boolean;
  declare private showAccessibilityAnnotatorSettingsLink_: boolean;

  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (!this.enableYourSavedInfoPolicyAndExtentionToggleIndicators_) {
      this.entityDataManager_.getOptInStatus().then(enhancedAutofillOptedIn => {
        this.set(
            'enhancedAutofillOptedIn_.value',
            this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn);
      });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.entityInstancesChangedListener_) {
      this.entityDataManager_.removeEntityInstancesChangedListener(
          this.entityInstancesChangedListener_);
      this.entityInstancesChangedListener_ = null;
    }
  }

  private getFirstWhenOnSectionTitle_() {
    return this.i18n(
        this.autofillAiAvailableByDefault_ ?
            'autofillAiWhenOnCanFillDifficultFields' :
            'autofillAiWhenOnSavedInfo');
  }

  private getFirstWhenOnSectionIcon_() {
    return this.autofillAiAvailableByDefault_ ? 'settings20:text-analysis' :
                                                'settings20:sync-saved-locally';
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

  private onAccessibilityAnnotatorSettingsLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('accessibilityAnnotatorSettingsUrl'));
    MetricsBrowserProxyImpl.getInstance().recordAction(
        'Autofill.Settings.AccessibilityAnnotatorSettingsLinkRowClick');
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
    // TODO(crbug.com/490338056): replace undefined check with pref
    // initialization check
    const addressAutofillEnabled = this.get('prefs.autofill.profile_enabled');

    if (this.enableYourSavedInfoPolicyAndExtentionToggleIndicators_ &&
        !this.autofillAddOtherDatatypesPrefIsEnabled_) {
      if (!!addressAutofillEnabled &&
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
    }

    await CrSettingsPrefs.initialized;
    const autofillAiPolicyValue =
        this.getPref(AiEnterpriseFeaturePrefName.AUTOFILL_AI).value;

    if (autofillAiPolicyValue === undefined) {
      return;
    }

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

      if (this.autofillAddOtherDatatypesPrefIsEnabled_) {
        this.set(
            'enhancedAutofillOptedIn_.value',
            this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn);
      } else {
        this.set(
            'enhancedAutofillOptedIn_.value',
            this.enhancedAutofillEligibleUser_ && enhancedAutofillOptedIn &&
                !!addressAutofillEnabled && addressAutofillEnabled.value);
      }
    }
  }

  private showExtensionControlledIndicator_() {
    if (!this.enableYourSavedInfoPolicyAndExtentionToggleIndicators_) {
      return false;
    }

    // TODO(crbug.com/490338056): replace undefined check with pref
    // initialization check
    const addressAutofillEnabled = this.get('prefs.autofill.profile_enabled');

    // We show the extension control only if extension forces false value
    return !!addressAutofillEnabled && !!addressAutofillEnabled.extensionId &&
        !addressAutofillEnabled.value;
  }

  private optInToggleDisabled_(
      addressAutofillEnabled?: chrome.settingsPrivate.PrefObject<boolean>):
      boolean {
    if (this.enableYourSavedInfoPolicyAndExtentionToggleIndicators_) {
      if (addressAutofillEnabled === undefined) {
        return true;
      }
      const addressAutofillEnforcedFalse =
          addressAutofillEnabled.enforcement ===
              chrome.settingsPrivate.Enforcement.ENFORCED &&
          !addressAutofillEnabled.value;
      // We need to check addressAutofillEnabled.value here.
      // this.enhancedAutofillEligibleUser_ does consider
      // addressAutofillEnabled.value, but loadTimeData constants are refreshed
      // only after page reload.
      return !this.enhancedAutofillEligibleUser_ ||
          addressAutofillEnforcedFalse;
    } else {
      return !this.enhancedAutofillEligibleUser_;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'collapsible-autofill-settings-card': CollapsibleCardElement;
  }
}

customElements.define(CollapsibleCardElement.is, CollapsibleCardElement);
