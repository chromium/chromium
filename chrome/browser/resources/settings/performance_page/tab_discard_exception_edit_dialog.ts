// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {PrefsMixin, PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';
import {HighEfficiencyModeExceptionListAction, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './tab_discard_exception_edit_dialog.html.js';
import {TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionValidationMixin, TabDiscardExceptionValidationMixinInterface} from './tab_discard_exception_validation_mixin.js';

export interface TabDiscardExceptionEditDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: CrInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const TabDiscardExceptionEditDialogElementBase =
    TabDiscardExceptionValidationMixin(PrefsMixin(PolymerElement)) as
    Constructor<TabDiscardExceptionValidationMixinInterface&PrefsMixinInterface&
                PolymerElement>;

export class TabDiscardExceptionEditDialogElement extends
    TabDiscardExceptionEditDialogElementBase {
  static get is() {
    return 'tab-discard-exception-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ruleToEdit: {type: String, value: ''},
    };
  }

  private browserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();
  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();
  private ruleToEdit: string;

  override ready() {
    super.ready();
    this.rule = this.ruleToEdit;
    this.submitDisabled = false;
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private async onSubmitClick_() {
    this.$.dialog.close();
    const rule = this.rule.trim();
    if (rule !== this.ruleToEdit) {
      if (!await this.browserProxy_.validateTabDiscardExceptionRule(rule)) {
        return;
      }
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

  setRuleToEditForTesting(rule: string) {
    this.ruleToEdit = rule;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-edit-dialog': TabDiscardExceptionEditDialogElement;
  }
}

customElements.define(
    TabDiscardExceptionEditDialogElement.is,
    TabDiscardExceptionEditDialogElement);
