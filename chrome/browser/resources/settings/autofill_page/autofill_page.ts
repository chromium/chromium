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
// <if expr="is_chromeos">
import '/shared/settings/controls/password_prompt_dialog.js';
// </if>
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './autofill_page.html.js';
import {PasswordCheckMixin} from './password_check_mixin.js';
import {PasswordManagerImpl, PasswordManagerPage} from './password_manager_proxy.js';
import {PasswordRequestorMixin} from './password_requestor_mixin.js';
import {PasswordViewPageInteractions, PasswordViewPageRequestedEvent, PasswordViewPageUrlParams, recordPasswordViewInteraction} from './password_view.js';

const SettingsAutofillPageElementBase = PrefsMixin(
    PasswordCheckMixin(PasswordRequestorMixin(BaseMixin(PolymerElement))));

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
      passwordFilter_: String,
      passkeyFilter_: String,

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
        },
      },

      enableNewPasswordManagerPage_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableNewPasswordManagerPage');
        },
      },

      // The credential is only used to pass the credential from password-view
      // to settings-subpage
      credential: {
        type: Object,
        value: null,
      },

      passwordsTitle_: {
        type: String,
        value() {
          return loadTimeData.getBoolean('enableNewPasswordManagerPage') ?
              loadTimeData.getString('localPasswordManager') :
              loadTimeData.getString('passwords');
        },
      },
    };
  }

  private passwordFilter_: string;
  private passkeyFilter_: string;
  private focusConfig_: Map<string, string>;
  private passwordManagerSubLabel_: string;
  private enablePasswordViewPage_: string;
  private enableNewPasswordManagerPage_: boolean;
  credential: chrome.passwordsPrivate.PasswordUiEntry|null;

  // <if expr="is_chromeos">
  override onPasswordPromptClose(event: CloseEvent) {
    super.onPasswordPromptClose(event);
    if (!this.tokenObtained &&
        Router.getInstance().getCurrentRoute() === routes.PASSWORD_VIEW) {
      Router.getInstance().navigateTo(routes.PASSWORDS);
    }
  }
  // </if>

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
    if (this.enableNewPasswordManagerPage_) {
      PasswordManagerImpl.getInstance().showPasswordManager(
          PasswordManagerPage.PASSWORDS);
      return;
    }
    Router.getInstance().navigateTo(routes.PASSWORDS);
  }

  /**
   * @return The sub-title message indicating the result of password check.
   */
  private computePasswordManagerSubLabel_(): string {
    if (this.enableNewPasswordManagerPage_) {
      return '';
    }
    return this.leakedPasswords.length > 0 ? this.compromisedPasswordsCount :
                                             '';
  }

  private onPasswordViewPageRequested_(event: PasswordViewPageRequestedEvent):
      void {
    const id = event.detail.entry.id;

    this.requestCredentialDetails(id)
        .then((passwordUiEntry: chrome.passwordsPrivate.PasswordUiEntry) => {
          this.credential = passwordUiEntry;
          recordPasswordViewInteraction(
              PasswordViewPageInteractions.CREDENTIAL_FOUND);
          if (Router.getInstance().getCurrentRoute() !== routes.PASSWORD_VIEW) {
            // If the current route is not |routes.PASSWORD_VIEW|, then the
            // credential is requested due to a row click in passwords list.
            // Route to the view page to display the credential.
            const params = new URLSearchParams();
            params.set(PasswordViewPageUrlParams.ID, String(id));
            Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);
          }
          PasswordManagerImpl.getInstance().extendAuthValidity();
        })
        .catch(() => {
          if (Router.getInstance().getCurrentRoute() === routes.PASSWORD_VIEW) {
            // If the credential is requested from the view page and is not
            // retrieved, return to |routes.PASSWORDS|. There is nothing to show
            // in |routes.PASSWORD_VIEW|.
            recordPasswordViewInteraction(
                PasswordViewPageInteractions.CREDENTIAL_NOT_FOUND);
            Router.getInstance().navigateTo(routes.PASSWORDS);
          }
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-page': SettingsAutofillPageElement;
  }
}

customElements.define(
    SettingsAutofillPageElement.is, SettingsAutofillPageElement);
