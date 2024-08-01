// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {convertDateToWindowsEpoch} from '../../time.js';
import type {PerformanceMetricsProxy} from '../performance_metrics_proxy.js';
import {MemorySaverModeExceptionListAction, PerformanceMetricsProxyImpl} from '../performance_metrics_proxy.js';

import {getTemplate} from './exception_edit_input.html.js';
import type {ExceptionValidationMixinInterface} from './exception_validation_mixin.js';
import {ExceptionValidationMixin, TAB_DISCARD_EXCEPTIONS_PREF} from './exception_validation_mixin.js';

export interface ExceptionEditInputElement {
  $: {
    input: CrInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const ExceptionEditInputElementBase =
    ExceptionValidationMixin(
        ListPropertyUpdateMixin(PrefsMixin(PolymerElement))) as
    Constructor<ExceptionValidationMixinInterface&
                ListPropertyUpdateMixinInterface&PrefsMixinInterface&
                PolymerElement>;

export class ExceptionEditInputElement extends
    ExceptionEditInputElementBase {
  static get is() {
    return 'tab-discard-exception-edit-input';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Represents the original rule that is being edited. When submit() is
       * called, it will be replaced by rule in the exception list.
       */
      ruleToEdit: {type: String, value: ''},
    };
  }

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private ruleToEdit: string;

  override ready() {
    super.ready();
    this.rule = this.ruleToEdit;
    this.submitDisabled = false;
  }

  submit() {
    assert(!this.submitDisabled);
    const rule = this.rule.trim();
    if (rule !== this.ruleToEdit) {
      this.deletePrefDictEntry(TAB_DISCARD_EXCEPTIONS_PREF, this.ruleToEdit);
      this.setPrefDictEntry(
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

customElements.define(
    ExceptionEditInputElement.is,
    ExceptionEditInputElement);
