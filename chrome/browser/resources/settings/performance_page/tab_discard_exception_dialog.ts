// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {PrefsMixin, PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';
import {HighEfficiencyModeExceptionListAction, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './tab_discard_exception_dialog.html.js';

export const MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH = 10 * 1024;
export const TAB_DISCARD_EXCEPTIONS_PREF =
    'performance_tuning.tab_discarding.exceptions';
export const TAB_DISCARD_EXCEPTIONS_MANAGED_PREF =
    'performance_tuning.tab_discarding.exceptions_managed';

export interface TabDiscardExceptionDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: CrInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const TabDiscardExceptionDialogElementBase =
    I18nMixin(PrefsMixin(PolymerElement)) as
    Constructor<I18nMixinInterface&PrefsMixinInterface&PolymerElement>;

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
      ruleToEdit: {type: String, value: ''},
      submitButtonAriaLabel_: String,
      submitButtonText_: String,
      submitDisabled_: Boolean,
      title_: String,
    };
  }

  private browserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();
  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();
  private errorMessage_: string;
  private inputInvalid_: boolean;
  private rule_: string;
  ruleToEdit: string;
  private submitButtonAriaLabel_: string;
  private submitButtonText_: string;
  private submitDisabled_: boolean;
  private title_: string;

  override ready() {
    super.ready();

    this.rule_ = this.ruleToEdit;
    // use initial value of rule to determine whether we are editing or adding
    if (this.rule_) {
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
    this.$.dialog.close();
    if (this.ruleToEdit) {
      // edit dialog
      if (this.rule_ !== this.ruleToEdit) {
        if (this.getPref(TAB_DISCARD_EXCEPTIONS_PREF)
                .value.includes(this.rule_)) {
          // delete instead of update, otherwise there would be a duplicate
          this.deletePrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, this.ruleToEdit);
        } else {
          this.updatePrefListItem(
              TAB_DISCARD_EXCEPTIONS_PREF, this.ruleToEdit, this.rule_);
        }
      }
      this.metricsProxy_.recordExceptionListAction(
          HighEfficiencyModeExceptionListAction.EDIT);
      return;
    }
    // add dialog
    this.appendPrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, this.rule_);
    this.dispatchEvent(new CustomEvent('add-exception', {
      bubbles: true,
      composed: true,
    }));
    this.metricsProxy_.recordExceptionListAction(
        HighEfficiencyModeExceptionListAction.ADD);
  }

  private validate_() {
    const rule = this.rule_.trim();

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
