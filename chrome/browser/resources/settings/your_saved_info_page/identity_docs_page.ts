// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-identity-docs-page', is a subpage of the "Your saved
 * info" section. It manages the user's autofill data for identity documents.
 * Users can add, edit, or delete their saved document details, as well as opt
 * out of the autofill functionality entirely.
 */

import '/shared/settings/prefs/prefs.js';
import '../autofill_page/autofill_ai_entries_list.js';
import '../autofill_page/your_saved_info_shared.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import type {EntityDataManagerProxy} from '../autofill_page/entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../autofill_page/entity_data_manager_proxy.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './identity_docs_page.html.js';

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

      enhancedAutofillOptedIn_: {
        type: Boolean,
        value: false,
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
        computed:
            'computeIdentityDocsOptedIn_(enhancedAutofillEligibleUser_, ' +
            'enhancedAutofillOptedIn_, ' +
            'prefs.autofill.autofill_ai.identity_entities_enabled, ' +
            'prefs.autofill.profile_enabled.value)',
      },

      /**
        If true, Autofill AI does not depend on whether Autofill for addresses
        is enabled.
      */
      // TODO(crbug.com/466345561): remove when enhanced autofill will stop
      // depending on addresses autofill
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
      'onAutofillOptInStatusChange_(prefs.autofill.autofill_ai.opt_in_status)',
    ];
  }

  declare private enhancedAutofillEligibleUser_: boolean;
  declare private enhancedAutofillOptedIn_: boolean;
  declare private identityDocsOptedIn_: chrome.settingsPrivate.PrefObject;
  declare private autofillAiIgnoresWhetherAddressFillingIsEnabled_: boolean;

  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();


  override connectedCallback() {
    super.connectedCallback();
  }

  private optInToggleDisabled_(): boolean {
    const addressAutofillOptInStatus =
        this.getPref<boolean>('autofill.profile_enabled').value;
    const ignoreAddressAutofill =
        this.autofillAiIgnoresWhetherAddressFillingIsEnabled_;

    // The identity docs opt-in toggle should be enabled (editable) when all
    // conditions are met:
    //  * User is eligible for enhanced autofill.
    //  * User is enrolled in enhanced autofill.
    //  * User is enrolled in address autofill (unless the experiment
    //    to ignore address autofill is active).
    const optInToggleEnabled = this.enhancedAutofillEligibleUser_ &&
        this.enhancedAutofillOptedIn_ &&
        (ignoreAddressAutofill || addressAutofillOptInStatus);

    return !optInToggleEnabled;
  }

  private onAutofillOptInStatusChange_() {
    this.entityDataManager_.getOptInStatus().then(status => {
      this.set('enhancedAutofillOptedIn_', status);
    });
  }

  private computeIdentityDocsOptedIn_():
      chrome.settingsPrivate.PrefObject<boolean> {
    const fakePref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'fake',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: this.getPref<boolean>(
                     'autofill.autofill_ai.identity_entities_enabled')
                 .value,
    };

    if (this.optInToggleDisabled_()) {
      fakePref.value = false;
    }

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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-identity-docs-page': SettingsIdentityDocsPageElement;
  }
}

customElements.define(
    SettingsIdentityDocsPageElement.is, SettingsIdentityDocsPageElement);
