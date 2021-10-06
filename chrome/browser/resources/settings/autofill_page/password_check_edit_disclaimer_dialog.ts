// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

interface SettingsPasswordEditDisclaimerDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsPasswordEditDisclaimerDialogElementBase =
    I18nMixin(PolymerElement);

class SettingsPasswordEditDisclaimerDialogElement extends
    SettingsPasswordEditDisclaimerDialogElementBase {
  static get is() {
    return 'settings-password-edit-disclaimer-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The website origin that is being displayed.
       */
      origin: String,
    };
  }

  origin: string;

  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onEditClick_() {
    this.dispatchEvent(new CustomEvent(
        'edit-password-click', {bubbles: true, composed: true}));
    this.$.dialog.close();
  }

  private onCancel_() {
    this.$.dialog.close();
  }

  private getDisclaimerTitle_(): string {
    return this.i18n('editDisclaimerTitle', this.origin);
  }
}

customElements.define(
    SettingsPasswordEditDisclaimerDialogElement.is,
    SettingsPasswordEditDisclaimerDialogElement);
