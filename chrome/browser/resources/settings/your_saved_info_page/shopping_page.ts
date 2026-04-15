// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-shopping-page', is a subpage of the "Your saved info"
 * section. It manages the user's autofill data for shopping. Users can view and
 * hide their saved orders and shipments as well as opt out of the autofill
 * functionality entirely.
 */

import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/prefs/prefs.js';
import '../autofill_page/autofill_ai_entries_list.js';
import '../autofill_page/your_saved_info_shared.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName} from '../ai_page/constants.js';
import type {ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import type {EntityDataManagerProxy} from '../autofill_page/entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../autofill_page/entity_data_manager_proxy.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {checkAutofillPoliciesAndModifyPrefIfNecessary} from './policy_utils.js';
import {getTemplate} from './shopping_page.html.js';

export interface SettingsShoppingPageElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

const SettingsShoppingPageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

export class SettingsShoppingPageElement extends
    SettingsShoppingPageElementBase {
  static get is() {
    return 'settings-shopping-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enhancedAutofillEligibleUser_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('userEligibleForAutofillAi');
        },
      },

      enhancedAutofillOptedIn_: {
        type: Boolean,
        value: false,
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

      canEnableOrDisableAutofillAi_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('canEnableOrDisableAutofillAi');
        },
      },

      /**
         Fake preference used by `this.$.optInToggle`. Shows value of
         `autofill.autofill_ai.shopping_entities_enabled` preference if toggle
         is enabled (clickable). If toggle is disabled then the value is
         overridden to be shown as false even if the preference is true.
       */
      shoppingOptedIn_: {
        type: Object,
        computed: `computeShoppingOptedIn_(enhancedAutofillEligibleUser_,
              enhancedAutofillOptedIn_,
              prefs.autofill.autofill_ai.shopping_entities_enabled,
              prefs.autofill.profile_enabled.value,
              prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI},
              prefsInitialized_)`,
      },

      /**
        If true, Autofill AI does not depend on whether Autofill for addresses
        is enabled.
      */
      // TODO(crbug.com/466345561): remove when enhanced autofill will stop
      // depending on addresses autofill
      autofillAddOtherDatatypesPrefIsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'AutofillAddOtherDatatypesPrefIsEnabled');
        },
      },

      enableYourSavedInfoPolicyAndExtentionToggleIndicators_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableYourSavedInfoPolicyAndExtentionToggleIndicators');
        },
      },

      /**
       * Set to true once CrSettingsPrefs is fully initialized.
       * Guards against race conditions where prefs are accessed before the full
       * preference tree is populated.
       */
      prefsInitialized_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'onAutofillOptInStatusChange_(prefs.autofill.autofill_ai.opt_in_status)',
    ];
  }

  declare private enhancedAutofillEligibleUser_: boolean;
  declare private enhancedAutofillOptedIn_: boolean;
  declare private shoppingOptedIn_: chrome.settingsPrivate.PrefObject;
  declare private autofillAddOtherDatatypesPrefIsEnabled_: boolean;
  declare private autofillAiAvailableByDefault_: boolean;
  declare private canEnableOrDisableAutofillAi_: boolean;
  declare private enableYourSavedInfoPolicyAndExtentionToggleIndicators_:
      boolean;
  declare private prefsInitialized_: boolean;

  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    CrSettingsPrefs.initialized.then(() => {
      this.prefsInitialized_ = true;
    });
  }

  private optInToggleDisabled_(): boolean {
    if (!this.prefsInitialized_) {
      return true;
    }

    const addressAutofillOptInStatus =
        this.getPref<boolean>('autofill.profile_enabled').value;
    const ignoreAddressAutofill = this.autofillAddOtherDatatypesPrefIsEnabled_;
    if (this.autofillAiAvailableByDefault_) {
      return !this.canEnableOrDisableAutofillAi_ ||
          (!ignoreAddressAutofill && !addressAutofillOptInStatus);
    }

    const optInToggleEnabled = this.enhancedAutofillEligibleUser_ &&
        this.enhancedAutofillOptedIn_ &&
        (ignoreAddressAutofill || addressAutofillOptInStatus);

    return !optInToggleEnabled;
  }

  private onAutofillOptInStatusChange_() {
    if (this.autofillAiAvailableByDefault_) {
      return;
    }
    this.entityDataManager_.getOptInStatus().then(status => {
      this.set('enhancedAutofillOptedIn_', status);
    });
  }

  private computeShoppingOptedIn_():
      chrome.settingsPrivate.PrefObject<boolean> {
    const fakePref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'fake',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };

    if (!this.prefsInitialized_) {
      return fakePref;
    }

    fakePref.value =
        this.getPref<boolean>('autofill.autofill_ai.shopping_entities_enabled')
            .value;

    if (this.optInToggleDisabled_()) {
      fakePref.value = false;
    }

    if (this.enableYourSavedInfoPolicyAndExtentionToggleIndicators_) {
      const addressPolicy = this.getPref<boolean>('autofill.profile_enabled');
      const autofillAiPolicy =
          this.getPref<ModelExecutionEnterprisePolicyValue>(
              AiEnterpriseFeaturePrefName.AUTOFILL_AI);

      checkAutofillPoliciesAndModifyPrefIfNecessary(
          fakePref, addressPolicy, autofillAiPolicy);
    }

    return fakePref;
  }

  private onOptInToggleChange_() {
    this.setPrefValue(
        'autofill.autofill_ai.shopping_entities_enabled',
        this.$.optInToggle.checked);
  }

  private getAllowedEntityTypes_(): Set<EntityTypeName> {
    return new Set([EntityTypeName.kOrder, EntityTypeName.kShipment]);
  }

  private extensionControlledIndicatorIsVisible_(): boolean {
    if (!this.enableYourSavedInfoPolicyAndExtentionToggleIndicators_ ||
        !this.prefsInitialized_) {
      return false;
    }

    const addressAutofillEnabled =
        this.getPref<boolean>('autofill.profile_enabled');

    return !!addressAutofillEnabled.extensionId &&
        !addressAutofillEnabled.value;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-shopping-page': SettingsShoppingPageElement;
  }
}

customElements.define(
    SettingsShoppingPageElement.is, SettingsShoppingPageElement);
