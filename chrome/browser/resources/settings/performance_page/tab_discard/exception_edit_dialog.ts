// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './exception_edit_input.js';

import type {PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './exception_edit_dialog.html.js';
import type {ExceptionEditInputElement} from './exception_edit_input.js';

export interface ExceptionEditDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: ExceptionEditInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const ExceptionEditDialogElementBase = PrefsMixin(PolymerElement) as
    Constructor<PrefsMixinInterface&PolymerElement>;

export class ExceptionEditDialogElement extends
    ExceptionEditDialogElementBase {
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

  private ruleToEdit: string;

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    this.$.dialog.close();
    this.$.input.submit();
  }

  setRuleToEditForTesting(rule: string) {
    this.ruleToEdit = rule;
    this.$.input.setRuleToEditForTesting();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-edit-dialog': ExceptionEditDialogElement;
  }
}

customElements.define(
    ExceptionEditDialogElement.is,
    ExceptionEditDialogElement);
