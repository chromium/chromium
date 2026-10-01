// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-travel-page', is a subpage of the "Your saved info"
 * section. It manages the user's autofill data for traveling. Users can add,
 * edit, or delete their saved document details, as well as opt out of the
 * autofill functionality entirely.
 */

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import '../autofill_ai_entries_list.js';
import '../autofill_shared.css.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName} from '../../ai_page/constants.js';
import type {ModelExecutionEnterprisePolicyValue} from '../../ai_page/constants.js';
import {EntityTypeName} from '../../autofill_ai_enums.mojom-webui.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiEntryPoint} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import {Router} from '../../router.js';
import {SettingsViewMixin} from '../../settings_page/settings_view_mixin.js';
import type {EntityDataManagerProxy} from '../entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../entity_data_manager_proxy.js';
import {AutofillPolicyDataCategory, checkAutofillPoliciesAndModifyPrefIfNecessary} from '../policy_utils.js';
import type {TypesBlockedEntry} from '../policy_utils.js';

import {getTemplate} from './travel_page.html.js';

export interface SettingsTravelPageElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

const SettingsTravelPageElementBase =
    SettingsViewMixin(PrefServiceObserverMixin(PolymerElement));

export class SettingsTravelPageElement extends SettingsTravelPageElementBase {
  static get is() {
    return 'settings-travel-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       Controls whether the user can use Autofill AI (in this context travel
       info filling). As an example, this can be false if the extensions API
       disables the feature.
      */
      canEnableOrDisableAutofillAi_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('canEnableOrDisableAutofillAi');
        },
      },

      /**
         Fake preference used by `this.$.optInToggle`. Shows value of
         `autofill.autofill_ai.travel_entities_enabled` preference if toggle
         is enabled (clickable). If toggle is disabled then the value is
         overridden to be shown as false even if the preference is true.
       */
      travelOptedIn_: {
        type: Object,
        value: () => ({
          key: 'fake',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        }),
      },

      /**
        If true, Autofill AI does not depend on whether Autofill for addresses
        is enabled.
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

      showSuggestionsFromGeminiSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showSuggestionsFromGeminiSettings');
        },
      },
    };
  }

  declare private travelOptedIn_: chrome.settingsPrivate.PrefObject<boolean>;
  declare private autofillSettingsEnterprisePolicyEnabled_: boolean;
  declare private canEnableOrDisableAutofillAi_: boolean;
  declare private profileEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|null;
  declare private showSuggestionsFromGeminiSettings_: boolean;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref('autofill.profile_enabled', 'profileEnabledPref_');

    const updateOptedIn = () => this.updateTravelOptedIn_();
    this.addPrefObserver(
        'autofill.autofill_ai.travel_entities_enabled', updateOptedIn);
    this.addPrefObserver('autofill.profile_enabled', updateOptedIn);
    this.addPrefObserver(
        AiEnterpriseFeaturePrefName.AUTOFILL_AI, updateOptedIn);
    this.addPrefObserver('autofill.types_blocked', updateOptedIn);

    this.entityDataManager_.preloadDetailsForUpsertPass();
  }

  private optInToggleDisabled_(): boolean {
    if (!this.profileEnabledPref_) {
      return true;
    }

    const addressAutofillOptInStatus = this.profileEnabledPref_.value;
    const ignoreAddressAutofill = this.autofillSettingsEnterprisePolicyEnabled_;
    return !this.canEnableOrDisableAutofillAi_ ||
        (!ignoreAddressAutofill && !addressAutofillOptInStatus);
  }

  private updateTravelOptedIn_() {
    const prefService = PrefService.getInstance();
    const fakePref: chrome.settingsPrivate.PrefObject<boolean> = {
      ...this.travelOptedIn_,
      value:
          prefService
              .getPref<boolean>('autofill.autofill_ai.travel_entities_enabled')
              .value,
    };

    if (this.optInToggleDisabled_()) {
      fakePref.value = false;
    }

    assert(this.profileEnabledPref_);
    const autofillAiPolicy =
        prefService.getPref<ModelExecutionEnterprisePolicyValue>(
            AiEnterpriseFeaturePrefName.AUTOFILL_AI);

    checkAutofillPoliciesAndModifyPrefIfNecessary(
        fakePref, this.profileEnabledPref_, autofillAiPolicy,
        prefService.getPref<TypesBlockedEntry[]>('autofill.types_blocked'),
        AutofillPolicyDataCategory.TRAVEL);

    this.travelOptedIn_ = fakePref;
  }

  private onOptInToggleChange_() {
    // If the preference is enforced by enterprise policy, do not allow the user
    // to toggle or mutate the underlying preference value.
    if (this.$.optInToggle.pref?.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      return;
    }
    PrefService.getInstance().setPrefValue(
        'autofill.autofill_ai.travel_entities_enabled',
        this.$.optInToggle.checked);
  }

  private getAllowedEntityTypes_(): Set<EntityTypeName> {
    return new Set([
      EntityTypeName.kFlightReservation,
      EntityTypeName.kKnownTravelerNumber,
      EntityTypeName.kRedressNumber,
      EntityTypeName.kVehicle,
    ]);
  }

  private getMetricEntityTypes_(): Record<EntityTypeName, string> {
    return {
      [EntityTypeName.kFlightReservation]: 'FlightReservation',
      [EntityTypeName.kKnownTravelerNumber]: 'KnownTravelerNumber',
      [EntityTypeName.kRedressNumber]: 'RedressNumber',
      [EntityTypeName.kVehicle]: 'Vehicle',
    } as Record<EntityTypeName, string>;
  }

  private extensionControlledIndicatorIsVisible_(): boolean {
    if (!this.profileEnabledPref_) {
      return false;
    }

    return !!this.profileEnabledPref_.extensionId &&
        !this.profileEnabledPref_.value;
  }

  private onSuggestionsFromGeminiClick_() {
    this.metricsBrowserProxy_.recordSuggestionsFromGeminiEntryPointClick(
        SuggestionsFromGeminiEntryPoint.TRAVEL);
    Router.getInstance().navigateTo(routes.SUGGESTIONS_FROM_GEMINI);
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();
    if (routes.SUGGESTIONS_FROM_GEMINI) {
      map.set(
          routes.SUGGESTIONS_FROM_GEMINI.path, '#suggestionsFromGeminiLinkRow');
    }
    return map;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-travel-page': SettingsTravelPageElement;
  }
}

customElements.define(SettingsTravelPageElement.is, SettingsTravelPageElement);
