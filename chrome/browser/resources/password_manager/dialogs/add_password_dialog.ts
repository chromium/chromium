// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../shared_style.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getTemplate} from './add_password_dialog.html.js';

export interface AddPasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    websiteInput: CrInputElement,
  };
}

const AddPasswordDialogElementBase = I18nMixin(PolymerElement);

export class AddPasswordDialogElement extends AddPasswordDialogElementBase {
  static get is() {
    return 'add-password-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      website_: String,

      /**
       * Error message if the website input is invalid.
       */
      websiteErrorMessage_: {type: String, value: null},
    };
  }

  private website_: string;
  private websiteErrorMessage_: string|null;

  private onCancel_() {
    this.$.dialog.close();
  }

  /**
   * Helper function that checks whether the entered url is valid.
   */
  private async validateWebsite_() {
    PasswordManagerImpl.getInstance()
        .getUrlCollection(this.website_)
        .then(urlCollection => {
          this.websiteErrorMessage_ = null;
          if (!urlCollection) {
            this.websiteErrorMessage_ = this.i18n('notValidWebsite');
          }
        })
        .catch(() => this.websiteErrorMessage_ = this.i18n('notValidWebsite'));
  }

  private onWebsiteInputBlur_() {
    if (!this.websiteErrorMessage_ && !this.website_.includes('.')) {
      this.websiteErrorMessage_ =
          this.i18n('missingTLD', `${this.website_}.com`);
    }
  }

  private isWebsiteInputInvalid_(): boolean {
    return !!this.websiteErrorMessage_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'add-password-dialog': AddPasswordDialogElement;
  }
}

customElements.define(AddPasswordDialogElement.is, AddPasswordDialogElement);
