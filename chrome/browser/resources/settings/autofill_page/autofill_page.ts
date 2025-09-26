// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/prefs/prefs.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';
// </if>
// <if expr="not _google_chrome">
import '../icons.html.js';

// </if>
// clang-format on



import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {AutofillSettingsReferrer, type MetricsBrowserProxy, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './autofill_page.html.js';
import {PasswordManagerImpl, PasswordManagerPage} from './password_manager_proxy.js';

const SettingsAutofillPageElementBase =
    SettingsViewMixin(PrefsMixin(I18nMixin(PolymerElement)));

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
      autofillAiAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showAutofillAiControl');
        },
      },
    };
  }

  declare private autofillAiAvailable_: boolean;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  /**
   * Shows the manage addresses sub page.
   */
  private onAddressesClick_() {
    this.metricsBrowserProxy_.recordAutofillSettingsReferrer(
        'Autofill.AddressesSettingsPage.VisitReferrer',
        AutofillSettingsReferrer.AUTOFILL_AND_PASSWORDS_PAGE);
    Router.getInstance().navigateTo(routes.ADDRESSES);
  }

  /**
   * Shows the manage payment methods sub page.
   */
  private onPaymentsClick_() {
    this.metricsBrowserProxy_.recordAutofillSettingsReferrer(
        'Autofill.PaymentMethodsSettingsPage.VisitReferrer',
        AutofillSettingsReferrer.AUTOFILL_AND_PASSWORDS_PAGE);
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
    this.metricsBrowserProxy_.recordAutofillSettingsReferrer(
        'Autofill.FormsAiSettingsPage.VisitReferrer',
        AutofillSettingsReferrer.AUTOFILL_AND_PASSWORDS_PAGE);
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

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();
    if (routes.PAYMENTS) {
      map.set(routes.PAYMENTS.path, '#paymentManagerButton');
    }
    if (routes.ADDRESSES) {
      map.set(routes.ADDRESSES.path, '#addressesManagerButton');
    }

    return map;
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    const ids = [
      'addresses',
      'autofillAi',
      // <if expr="is_win or is_macosx">
      'passkeys',
      // </if>
      'payments',
    ];
    assert(ids.includes(childViewId));

    let triggerId: string|null = null;
    switch (childViewId) {
      case 'addresses':
        triggerId = 'addressesManagerButton';
        break;
      case 'autofillAi':
        assert(this.autofillAiAvailable_);
        triggerId = 'autofillAiManagerButton';
        break;
      // <if expr="is_win or is_macosx">
      case 'passkeys':
        triggerId = 'passwordManagerButton';
        break;
      // </if>
      case 'payments':
        triggerId = 'paymentManagerButton';
        break;
      default:
        break;
    }

    assert(triggerId);

    const control =
        this.shadowRoot!.querySelector<HTMLElement>(`#${triggerId}`);
    assert(control);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-page': SettingsAutofillPageElement;
  }
}

customElements.define(
    SettingsAutofillPageElement.is, SettingsAutofillPageElement);
