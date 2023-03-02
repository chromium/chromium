// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './kerberos_settings_redirect.html.js';


interface KerberosSettingsRedirectElement {
  $: {
    redirectDialog: CrDialogElement,
  };
}

const KerberosSettingsRedirectElementBase = I18nMixin(PolymerElement);

class KerberosSettingsRedirectElement extends
    KerberosSettingsRedirectElementBase {
  static get is() {
    return 'kerberos-settings-redirect';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.redirectDialog.showModal();
  }

  private onVisitWithoutTicket_(): void {
    chrome.send('dialogClose');
  }

  private onManageTickets_(): void {
    chrome.send('openSettings');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'kerberos-settings-redirect': KerberosSettingsRedirectElement;
  }
}

customElements.define(
    KerberosSettingsRedirectElement.is, KerberosSettingsRedirectElement);
