// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';
import {getTemplate} from './tab_discard_exception_dialog.html.js';

export const MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH = 10 * 1024;
export const SUBMIT_EVENT = 'submit';

export interface TabDiscardExceptionDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: CrInputElement,
  };
}

const TabDiscardExceptionDialogElementBase =
    I18nMixin(BaseMixin(PolymerElement));

export class TabDiscardExceptionDialogElement extends
    TabDiscardExceptionDialogElementBase {
  static get is() {
    return 'tab-discard-exception-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      errorMessage_: {type: String, value: ''},
      inputInvalid_: {type: Boolean, value: false},
      rule: {type: String, value: ''},
      submitButtonAriaLabel_: String,
      submitButtonText_: String,
      submitDisabled_: Boolean,
      title_: String,
    };
  }

  private browserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();
  private errorMessage_: string;
  private inputInvalid_: boolean;
  rule: string;
  private submitButtonAriaLabel_: string;
  private submitButtonText_: string;
  private submitDisabled_: boolean;
  private title_: string;

  override ready() {
    super.ready();

    // use initial value of rule to determine whether we are editing or adding
    if (this.rule) {
      this.title_ = this.i18n('editSiteTitle');
      this.submitButtonAriaLabel_ =
          this.i18n('tabDiscardingExceptionsSaveButtonAriaLabel');
      this.submitButtonText_ = this.i18n('save');
      this.submitDisabled_ = false;
    } else {
      this.title_ = this.i18n('addSiteTitle');
      this.submitButtonAriaLabel_ =
          this.i18n('tabDiscardingExceptionsAddButtonAriaLabel');
      this.submitButtonText_ = this.i18n('add');
      this.submitDisabled_ = true;
    }
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    this.fire(SUBMIT_EVENT, this.rule);
    this.$.dialog.close();
  }

  private validate_() {
    const rule = this.rule.trim();

    if (!rule) {
      this.inputInvalid_ = false;
      this.submitDisabled_ = true;
      this.errorMessage_ = '';
      return;
    }

    if (rule.length > MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH) {
      this.inputInvalid_ = true;
      this.submitDisabled_ = true;
      this.errorMessage_ = this.i18n('onStartupUrlTooLong');
      return;
    }

    this.browserProxy_.validateTabDiscardExceptionRule(rule).then(valid => {
      this.$.input.invalid = !valid;
      this.submitDisabled_ = !valid;
      this.errorMessage_ = valid ? '' : this.i18n('onStartupInvalidUrl');
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-dialog': TabDiscardExceptionDialogElement;
  }
}

customElements.define(
    TabDiscardExceptionDialogElement.is, TabDiscardExceptionDialogElement);
