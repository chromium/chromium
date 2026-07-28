// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './exception_edit_input.js';

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

export class ExceptionEditDialogElement extends PolymerElement {
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

  declare private ruleToEdit: string;

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
