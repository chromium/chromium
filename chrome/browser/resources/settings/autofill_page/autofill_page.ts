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
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/prefs/prefs.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';
// </if>
// <if expr="not _google_chrome">
import '../icons.html.js';

// </if>

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './autofill_page.html.js';
import {PasswordManagerImpl, PasswordManagerPage} from './password_manager_proxy.js';
import {UserAnnotationsManagerProxyImpl} from './user_annotations_manager_proxy.js';

const SettingsAutofillPageElementBase =
    PrefsMixin(I18nMixin(BaseMixin(PolymerElement)));

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

      plusAddressIcon_: {
        type: String,
        value() {
          // <if expr="_google_chrome">
          return 'settings-internal:plus-address-logo-medium';
          // </if>
          // <if expr="not _google_chrome">
          return 'settings:email';
          // </if>
        },
      },

      userEligibleForAutofillAi_: {
        type: Boolean,
        value: false,
      },


      userHasAutofillAiEntries_: {
        type: Boolean,
        value: false,
      },

      autofillAiAvailable_: {
        type: Boolean,
        computed: 'computeAutofillAiAvailable_(userEligibleForAutofillAi_, ' +
            'userHasAutofillAiEntries_)',
      },
    };
  }

  private passkeyFilter_: string;
  private userEligibleForAutofillAi_: boolean;
  private userHasAutofillAiEntries_: boolean;
  private autofillAiAvailable_: boolean;
  private focusConfig_: Map<string, string>;

  override connectedCallback() {
    super.connectedCallback();
    // TODO(crbug.com/368565649): Consider updating on sign-in state changes.
    UserAnnotationsManagerProxyImpl.getInstance().isUserEligible().then(
        eligible => {
          this.userEligibleForAutofillAi_ = eligible;
        });
    UserAnnotationsManagerProxyImpl.getInstance().hasEntries().then(value => {
      this.userHasAutofillAiEntries_ = value;
    });
  }

  /**
   * Computes `autofillAiAvailable_`.
   */
  private computeAutofillAiAvailable_(): boolean {
    return loadTimeData.getBoolean('autofillAiEnabled') &&
        (this.userEligibleForAutofillAi_ || this.userHasAutofillAiEntries_);
  }


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

  /**
   * Shows the Autofill AI settings sub page.
   */
  private onAutofillAiClick_() {
    Router.getInstance().navigateTo(routes.AUTOFILL_AI);
  }

  /**
   * @returns the sublabel of the address entry.
   */
  private addressesSublabel_() {
    return loadTimeData.getBoolean('plusAddressEnabled') ?
        this.i18n('addressesSublabel') :
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
