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
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import type {EntityDataManagerProxy} from '../autofill_page/entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from '../autofill_page/entity_data_manager_proxy.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

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

      autofillAddOtherDatatypesPrefIsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'AutofillAddOtherDatatypesPrefIsEnabled');
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
  declare private autofillAddOtherDatatypesPrefIsEnabled_: boolean;
  declare private autofillAiAvailableByDefault_: boolean;
  declare private canEnableOrDisableAutofillAi_: boolean;

  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  private optInToggleDisabled_(): boolean {
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

  private onOptInToggleChange_() {
    // TODO(crbug.com/498179650): Hook in the shopping entities pref once it
    // exists.
  }

  private getAllowedEntityTypes_(): Set<EntityTypeName> {
    return new Set([EntityTypeName.kOrder, EntityTypeName.kShipment]);
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
