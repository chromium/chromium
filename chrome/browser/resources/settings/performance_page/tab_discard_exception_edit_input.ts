// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {PrefsMixin, PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {ListPropertyUpdateMixin, ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HighEfficiencyModeExceptionListAction, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './tab_discard_exception_edit_input.html.js';
import {TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionValidationMixin, TabDiscardExceptionValidationMixinInterface} from './tab_discard_exception_validation_mixin.js';

export interface TabDiscardExceptionEditInputElement {
  $: {
    input: CrInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const TabDiscardExceptionEditInputElementBase =
    TabDiscardExceptionValidationMixin(
        ListPropertyUpdateMixin(PrefsMixin(PolymerElement))) as
    Constructor<TabDiscardExceptionValidationMixinInterface&
                ListPropertyUpdateMixinInterface&PrefsMixinInterface&
                PolymerElement>;

export class TabDiscardExceptionEditInputElement extends
    TabDiscardExceptionEditInputElementBase {
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
      if (this.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value.includes(rule)) {
        // delete instead of update, otherwise there would be a duplicate
        this.deletePrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, this.ruleToEdit);
      } else {
        this.updatePrefListItem(
            TAB_DISCARD_EXCEPTIONS_PREF, this.ruleToEdit, rule);
      }
    }
    this.metricsProxy_.recordExceptionListAction(
        HighEfficiencyModeExceptionListAction.EDIT);
  }

  setRuleToEditForTesting() {
    this.rule = this.ruleToEdit;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-edit-input': TabDiscardExceptionEditInputElement;
  }
}

customElements.define(
    TabDiscardExceptionEditInputElement.is,
    TabDiscardExceptionEditInputElement);
