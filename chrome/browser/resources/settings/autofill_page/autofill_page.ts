// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-autofill-page' is the settings page containing settings for
 * passwords, payment methods and addresses.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './autofill_page.html.js';
import {PasswordManagerImpl, PasswordManagerPage} from './password_manager_proxy.js';

const SettingsAutofillPageElementBase = PrefsMixin(BaseMixin(PolymerElement));

export interface SettingsAutofillPageElement {
  $: {
    passwordManagerButton: CrLinkRowElement,
  };
}

export class SettingsAutofillPageElement extends
    SettingsAutofillPageElementBase {
  static get is() {
    return 'settings-autofill-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      passkeyFilter_: String,

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.PAYMENTS) {
            map.set(routes.PAYMENTS.path, '#paymentManagerButton');
          }
          if (routes.ADDRESSES) {
            map.set(routes.ADDRESSES.path, '#addressesManagerButton');
          }

          return map;
        },
      },
    };
  }

  private passkeyFilter_: string;
  private focusConfig_: Map<string, string>;

  /**
   * Shows the manage addresses sub page.
   */
  private onAddressesClick_() {
    Router.getInstance().navigateTo(routes.ADDRESSES);
  }

  /**
   * Shows the manage payment methods sub page.
   */
  private onPaymentsClick_() {
    Router.getInstance().navigateTo(routes.PAYMENTS);
  }

  /**
   * Shows Password Manager page.
   */
  private onPasswordsClick_() {
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.PASSWORDS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-page': SettingsAutofillPageElement;
  }
}

customElements.define(
    SettingsAutofillPageElement.is, SettingsAutofillPageElement);
