// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-autofill-page' is the settings page containing settings for
 * passwords, payment methods and addresses.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../prefs/prefs.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared_css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './autofill_page.html.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordCheckMixin} from './password_check_mixin.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

const SettingsAutofillPageElementBase =
    PrefsMixin(PasswordCheckMixin(BaseMixin(PolymerElement)));

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
      passwordFilter_: String,

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.PASSWORDS) {
            map.set(routes.PASSWORDS.path, '#passwordManagerButton');
          }
          if (routes.PAYMENTS) {
            map.set(routes.PAYMENTS.path, '#paymentManagerButton');
          }
          if (routes.ADDRESSES) {
            map.set(routes.ADDRESSES.path, '#addressesManagerButton');
          }

          return map;
        },
      },

      passwordManagerSubLabel_: {
        type: String,
        computed: 'computePasswordManagerSubLabel_(compromisedPasswordsCount)',
      },

      enablePasswordViewPage_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordViewPage');
        }
      },

      // The credential is only used to pass the credential from password-view
      // to settings-subpage
      credential: {
        type: Object,
        value: null,
      },
    };
  }

  private passwordFilter_: string;
  private focusConfig_: Map<string, string>;
  private passwordManagerSubLabel_: string;
  private enablePasswordViewPage_: string;
  credential: MultiStorePasswordUiEntry|null;

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
   * Shows a page to manage passwords.
   */
  private onPasswordsClick_() {
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    Router.getInstance().navigateTo(routes.PASSWORDS);
  }

  /**
   * @return The sub-title message indicating the result of password check.
   */
  private computePasswordManagerSubLabel_(): string {
    return this.leakedPasswords.length > 0 ? this.compromisedPasswordsCount :
                                             '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-page': SettingsAutofillPageElement;
  }
}

customElements.define(
    SettingsAutofillPageElement.is, SettingsAutofillPageElement);
