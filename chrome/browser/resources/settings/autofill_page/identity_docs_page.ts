// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-identity-docs-page', is a subpage of the "Your saved
 * info" section. It manages the user's autofill data for identity documents.
 * Users can add, edit, or delete their saved document details, as well as opt
 * out of the autofill functionality entirely.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/prefs/prefs.js';
import './autofill_ai_entries_list.js';
import './autofill_shared.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName} from '../ai_page/constants.js';
import type {ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiEntryPoint} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './identity_docs_page.html.js';
import {checkAutofillPoliciesAndModifyPrefIfNecessary} from './policy_utils.js';

export interface SettingsIdentityDocsPageElement {
  $: {
    optInToggle: SettingsToggleButtonElement,
  };
}

const SettingsIdentityDocsPageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

export class SettingsIdentityDocsPageElement extends
    SettingsIdentityDocsPageElementBase {
  static get is() {
    return 'settings-identity-docs-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {

      /**
       Controls whether the user can use Autofill AI (in this context, identity
       docs filling). As an example, this can be false if the extensions API
       disables the feature.
      */
      canEnableOrDisableAutofillAi_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('canEnableOrDisableAutofillAi');
        },
      },


      /**
         Fake preference used by `this.$.optInToggle`. Stores the value of
         the `autofill.autofill_ai.identity_entities_enabled` preference if
         the toggle is enabled (clickable). If the toggle is disabled, then the
         value is overridden to be shown as false even if the preference is
         true.
       */
      identityDocsOptedIn_: {
        type: Object,
        computed: `computeIdentityDocsOptedIn_(
              prefs.autofill.autofill_ai.identity_entities_enabled,
              prefs.autofill.profile_enabled.value,
              prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI},
              prefsInitialized_)`,
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

      prefsInitialized_: {
        type: Boolean,
        value: false,
      },

      showSuggestionsFromGeminiSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showSuggestionsFromGeminiSettings');
        },
      },
    };
  }

  declare private canEnableOrDisableAutofillAi_: boolean;
  declare private identityDocsOptedIn_: chrome.settingsPrivate.PrefObject;
  declare private autofillSettingsEnterprisePolicyEnabled_: boolean;
  declare private prefsInitialized_: boolean;
  declare private showSuggestionsFromGeminiSettings_: boolean;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

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
    const ignoreAddressAutofill = this.autofillSettingsEnterprisePolicyEnabled_;
    return !this.canEnableOrDisableAutofillAi_ ||
        (!ignoreAddressAutofill && !addressAutofillOptInStatus);
  }

  private computeIdentityDocsOptedIn_():
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
        this.getPref<boolean>('autofill.autofill_ai.identity_entities_enabled')
            .value;

    if (this.optInToggleDisabled_()) {
      fakePref.value = false;
    }

    const addressPolicy = this.getPref<boolean>('autofill.profile_enabled');
    const autofillAiPolicy = this.getPref<ModelExecutionEnterprisePolicyValue>(
        AiEnterpriseFeaturePrefName.AUTOFILL_AI);

    checkAutofillPoliciesAndModifyPrefIfNecessary(
        fakePref, addressPolicy, autofillAiPolicy);

    return fakePref;
  }

  private onOptInToggleChange_() {
    this.setPrefValue(
        'autofill.autofill_ai.identity_entities_enabled',
        this.$.optInToggle.checked);
  }

  private getAllowedEntityTypes_(): Set<EntityTypeName> {
    return new Set([
      EntityTypeName.kDriversLicense,
      EntityTypeName.kNationalIdCard,
      EntityTypeName.kPassport,
    ]);
  }

  private getMetricEntityTypes_(): Record<EntityTypeName, string> {
    return {
      [EntityTypeName.kDriversLicense]: 'DriversLicense',
      [EntityTypeName.kNationalIdCard]: 'NationalIdCard',
      [EntityTypeName.kPassport]: 'Passport',
    } as Record<EntityTypeName, string>;
  }

  private extensionControlledIndicatorIsVisible_(): boolean {
    if (!this.prefsInitialized_) {
      return false;
    }

    const addressAutofillEnabled =
        this.getPref<boolean>('autofill.profile_enabled');

    return !!addressAutofillEnabled.extensionId &&
        !addressAutofillEnabled.value;
  }

  private onSuggestionsFromGeminiClick_() {
    this.metricsBrowserProxy_.recordSuggestionsFromGeminiEntryPointClick(
        SuggestionsFromGeminiEntryPoint.IDENTITY_DOCS);
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
    'settings-identity-docs-page': SettingsIdentityDocsPageElement;
  }
}

customElements.define(
    SettingsIdentityDocsPageElement.is, SettingsIdentityDocsPageElement);
