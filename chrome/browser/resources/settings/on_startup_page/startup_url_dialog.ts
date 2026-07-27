// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getHtml} from './startup_url_dialog.html.js';
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

export class SettingsStartupUrlDialogElement extends CrLitElement {
  static get is() {
    return 'settings-startup-url-dialog';
  }

  static override get styles() {
    return [getSettingsSharedCss()];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      error_: {type: Number},
      url_: {type: String},
      urlLimit_: {type: Number},
      model: {type: Object},
      dialogTitle_: {type: String},
      actionButtonText_: {type: String},
    };
  }

  protected accessor error_: UrlInputError = UrlInputError.NONE;
  protected accessor url_: string = '';
  protected accessor urlLimit_: number = 100 * 1024;  // 100 KB.
  accessor model: StartupPageInfo|null = null;
  protected accessor dialogTitle_: string = '';
  protected accessor actionButtonText_: string = '';
  private browserProxy_: StartupUrlsPageBrowserProxy =
      StartupUrlsPageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    if (this.model) {
      this.dialogTitle_ = loadTimeData.getString('onStartupEditPage');
      this.actionButtonText_ = loadTimeData.getString('save');
      // Pre-populate the input field.
      this.url_ = this.model.url;
    } else {
      this.dialogTitle_ = loadTimeData.getString('onStartupAddNewPage');
      this.actionButtonText_ = loadTimeData.getString('add');
    }

    super.connectedCallback();
    this.$.actionButton.disabled = !this.model;
    this.$.dialog.showModal();
  }

  protected onUrlValueChanged_(e: CustomEvent<{value: string}>) {
    this.url_ = e.detail.value;
  }

  protected hasError_(): boolean {
    return this.error_ !== UrlInputError.NONE;
  }

  protected getErrorMessage_(): string {
    switch (this.error_) {
      case UrlInputError.INVALID_URL:
        return loadTimeData.getString('onStartupInvalidUrl');
      case UrlInputError.TOO_LONG:
        return loadTimeData.getString('onStartupUrlTooLong');
      default:
        return '';
    }
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected onActionButtonClick_() {
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

  protected onInput_() {
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
