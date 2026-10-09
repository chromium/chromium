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
import '../autofill_ai_entries_list.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AiEnterpriseFeaturePrefName} from '../../ai_page/constants.js';
import type {ModelExecutionEnterprisePolicyValue} from '../../ai_page/constants.js';
import {EntityTypeName} from '../../autofill_ai_enums.mojom-webui.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiEntryPoint} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import {Router} from '../../router.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';
import type {EntityDataManagerProxy} from '../entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../entity_data_manager_proxy.js';
import {AutofillPolicyDataCategory, checkAutofillPoliciesAndModifyPrefIfNecessary} from '../policy_utils.js';
import type {TypesBlockedEntry} from '../policy_utils.js';

import {getCss} from './travel_page.css.js';
import {getHtml} from './travel_page.html.js';

export interface SettingsTravelPageElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

const SettingsTravelPageElementBase =
    SettingsViewMixinLit(PrefServiceObserverMixinLit(CrLitElement));

export class SettingsTravelPageElement extends SettingsTravelPageElementBase {
  static get is() {
    return 'settings-travel-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      allowedEntityTypes_: {type: Object},

      /**
       * If true, Autofill AI does not depend on whether Autofill for addresses
       * is enabled.
       */
      autofillSettingsEnterprisePolicyEnabled_: {type: Boolean},

      /**
       * Controls whether the user can use Autofill AI (in this context travel
       * info filling). As an example, this can be false if the extensions API
       * disables the feature.
       */
      canEnableOrDisableAutofillAi_: {type: Boolean},

      metricEntityTypes_: {type: Object},
      profileEnabledPref_: {type: Object},
      showSuggestionsFromGeminiSettings_: {type: Boolean},

      /**
       * Fake preference used by `this.$.optInToggle`. Shows value of
       * `autofill.autofill_ai.travel_entities_enabled` preference if toggle
       * is enabled (clickable). If toggle is disabled then the value is
       * overridden to be shown as false even if the preference is true.
       */
      travelOptedIn_: {type: Object},
    };
  }

  protected accessor allowedEntityTypes_: Set<EntityTypeName> = new Set([
    EntityTypeName.kFlightReservation,
    EntityTypeName.kKnownTravelerNumber,
    EntityTypeName.kRedressNumber,
    EntityTypeName.kVehicle,
  ]);

  protected accessor metricEntityTypes_:
      Partial<Record<EntityTypeName, string>> = {
        [EntityTypeName.kFlightReservation]: 'FlightReservation',
        [EntityTypeName.kKnownTravelerNumber]: 'KnownTravelerNumber',
        [EntityTypeName.kRedressNumber]: 'RedressNumber',
        [EntityTypeName.kVehicle]: 'Vehicle',
      };

  private accessor canEnableOrDisableAutofillAi_: boolean =
      loadTimeData.getBoolean('canEnableOrDisableAutofillAi');

  protected accessor travelOptedIn_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'fake',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };

  private accessor autofillSettingsEnterprisePolicyEnabled_: boolean =
      loadTimeData.getBoolean('AutofillSettingsEnterprisePolicyEnabled');

  protected accessor profileEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|null = null;

  protected accessor showSuggestionsFromGeminiSettings_: boolean =
      loadTimeData.getBoolean('showSuggestionsFromGeminiSettings');

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

  protected optInToggleDisabled_(): boolean {
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

  protected onOptInToggleSettingsBooleanControlChange_() {
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

  protected extensionControlledIndicatorIsVisible_(): boolean {
    if (!this.profileEnabledPref_) {
      return false;
    }

    return !!this.profileEnabledPref_.extensionId &&
        !this.profileEnabledPref_.value;
  }

  protected onSuggestionsFromGeminiClick_() {
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
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-travel-page': SettingsTravelPageElement;
  }
}

customElements.define(SettingsTravelPageElement.is, SettingsTravelPageElement);
