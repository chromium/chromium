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

import {getTemplate} from './exception_add_input.html.js';
import type {ExceptionValidationMixinInterface} from './exception_validation_mixin.js';
import {ExceptionValidationMixin, TAB_DISCARD_EXCEPTIONS_PREF} from './exception_validation_mixin.js';

export interface ExceptionAddInputElement {
  $: {
    input: CrInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const ExceptionAddInputElementBase =
    ExceptionValidationMixin(
        ListPropertyUpdateMixin(PrefsMixin(PolymerElement))) as
    Constructor<ExceptionValidationMixinInterface&
                ListPropertyUpdateMixinInterface&PrefsMixinInterface&
                PolymerElement>;

export class ExceptionAddInputElement extends
    ExceptionAddInputElementBase {
  static get is() {
    return 'tab-discard-exception-add-input';
  }

  static get template() {
    return getTemplate();
  }

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  submit() {
    assert(!this.submitDisabled);
    const rule = this.rule.trim();
    this.setPrefDictEntry(
        TAB_DISCARD_EXCEPTIONS_PREF, rule, convertDateToWindowsEpoch());
    this.metricsProxy_.recordExceptionListAction(
        MemorySaverModeExceptionListAction.ADD_MANUAL);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-add-input': ExceptionAddInputElement;
  }
}

customElements.define(
    ExceptionAddInputElement.is, ExceptionAddInputElement);
