// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './kerberos_settings_redirect.html.js';

class KerberosSettingsRedirectElement extends PolymerElement {
  static get is() {
    return 'kerberos-settings-redirect';
  }

  static get template() {
    return getTemplate();
  }

  private onCancelButtonClicked_(): void {
    chrome.send('dialogClose');
  }

  private onManageTickets_(): void {
    chrome.send('dialogClose', ['openSettings']);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'kerberos-settings-redirect': KerberosSettingsRedirectElement;
  }
}

customElements.define(
    KerberosSettingsRedirectElement.is, KerberosSettingsRedirectElement);
