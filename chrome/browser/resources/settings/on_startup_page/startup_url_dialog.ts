// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '../settings_shared.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './startup_url_dialog.html.js';
import type {StartupPageInfo, StartupUrlsPageBrowserProxy} from './startup_urls_page_browser_proxy.js';
import {StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';


/**
 * Describe the current URL input error status.
 * @enum {number}
 */
enum UrlInputError {
  NONE = 0,
  INVALID_URL = 1,
  TOO_LONG = 2,
}

/**
 * @fileoverview 'settings-startup-url-dialog' is a component for adding
 * or editing a startup URL entry.
 */

export interface SettingsStartupUrlDialogElement {
  $: {
    actionButton: CrButtonElement,
    dialog: CrDialogElement,
    url: CrInputElement,
  };
}

export class SettingsStartupUrlDialogElement extends PolymerElement {
  static get is() {
    return 'settings-startup-url-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      error_: {
        type: Number,
        value: UrlInputError.NONE,
      },

      url_: String,

      urlLimit_: {
        readOnly: true,
        type: Number,
        value: 100 * 1024,  // 100 KB.
      },

      /**
       * If specified the dialog acts as an "Edit page" dialog, otherwise as an
       * "Add new page" dialog.
       */
      model: Object,

      dialogTitle_: String,

      actionButtonText_: String,

    };
  }

  private error_: UrlInputError;
  private url_: string;
  private urlLimit_: number;
  model: StartupPageInfo|null;
  private dialogTitle_: string;
  private actionButtonText_: string;
  private browserProxy_: StartupUrlsPageBrowserProxy =
      StartupUrlsPageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (this.model) {
      this.dialogTitle_ = loadTimeData.getString('onStartupEditPage');
      this.actionButtonText_ = loadTimeData.getString('save');
      this.$.actionButton.disabled = false;
      // Pre-populate the input field.
      this.url_ = this.model.url;
    } else {
      this.dialogTitle_ = loadTimeData.getString('onStartupAddNewPage');
      this.actionButtonText_ = loadTimeData.getString('add');
      this.$.actionButton.disabled = true;
    }
    this.$.dialog.showModal();
  }

  private hasError_(): boolean {
    return this.error_ !== UrlInputError.NONE;
  }

  private errorMessage_(invalidUrl: string, tooLong: string): string {
    return ['', invalidUrl, tooLong][this.error_];
  }

  private onCancelClick_() {
    this.$.dialog.close();
  }

  private onActionButtonClick_() {
    const whenDone = this.model ?
        this.browserProxy_.editStartupPage(this.model.modelIndex, this.url_) :
        this.browserProxy_.addStartupPage(this.url_);

    whenDone.then(success => {
      if (success) {
        this.$.dialog.close();
      }
      // If the URL was invalid, there is nothing to do, just leave the dialog
      // open and let the user fix the URL or cancel.
    });
  }

  private validate_() {
    if (this.url_.length === 0) {
      this.$.actionButton.disabled = true;
      this.error_ = UrlInputError.NONE;
      return;
    }
    if (this.url_.length >= this.urlLimit_) {
      this.$.actionButton.disabled = true;
      this.error_ = UrlInputError.TOO_LONG;
      return;
    }
    this.browserProxy_.validateStartupPage(this.url_).then(isValid => {
      this.$.actionButton.disabled = !isValid;
      this.error_ = isValid ? UrlInputError.NONE : UrlInputError.INVALID_URL;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-startup-url-dialog': SettingsStartupUrlDialogElement;
  }
}

customElements.define(
    SettingsStartupUrlDialogElement.is, SettingsStartupUrlDialogElement);
