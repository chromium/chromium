// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {convertDateToWindowsEpoch} from '../../time.js';
import type {PerformanceMetricsProxy} from '../performance_metrics_proxy.js';
import {MemorySaverModeExceptionListAction, PerformanceMetricsProxyImpl} from '../performance_metrics_proxy.js';

import {getHtml} from './exception_edit_input.html.js';
import {ExceptionValidationMixin, TAB_DISCARD_EXCEPTIONS_PREF} from './exception_validation_mixin.js';

export interface ExceptionEditInputElement {
  $: {
    input: CrInputElement,
  };
}

const ExceptionEditInputElementBase = ExceptionValidationMixin(CrLitElement);

export class ExceptionEditInputElement extends ExceptionEditInputElementBase {
  static get is() {
    return 'tab-discard-exception-edit-input';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...super.properties,
      /**
       * Represents the original rule that is being edited. When submit() is
       * called, it will be replaced by rule in the exception list.
       */
      ruleToEdit: {type: String},
    };
  }

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  accessor ruleToEdit: string = '';

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('ruleToEdit')) {
      this.rule = this.ruleToEdit;
      this.submitDisabled = false;
    }
  }

  protected onRuleValueChanged_(e: CustomEvent<{value: string}>) {
    this.rule = e.detail.value;
    this.validate();
  }

  submit() {
    assert(!this.submitDisabled);
    const rule = this.rule.trim();
    if (rule !== this.ruleToEdit) {
      PrefService.getInstance().deletePrefDictEntry(
          TAB_DISCARD_EXCEPTIONS_PREF, this.ruleToEdit);
      PrefService.getInstance().setPrefDictEntry(
          TAB_DISCARD_EXCEPTIONS_PREF, rule, convertDateToWindowsEpoch());
    }
    this.metricsProxy_.recordExceptionListAction(
        MemorySaverModeExceptionListAction.EDIT);
  }

  setRuleToEditForTesting() {
    this.rule = this.ruleToEdit;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-edit-input': ExceptionEditInputElement;
  }
}

customElements.define(ExceptionEditInputElement.is, ExceptionEditInputElement);
