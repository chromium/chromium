// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './exception_add_input.js';

import type {PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './exception_add_dialog.html.js';
import type {ExceptionAddInputElement} from './exception_add_input.js';

export interface ExceptionAddDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: ExceptionAddInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const ExceptionAddDialogElementBase = PrefsMixin(PolymerElement) as
    Constructor<PrefsMixinInterface&PolymerElement>;

export class ExceptionAddDialogElement extends
    ExceptionAddDialogElementBase {
  static get is() {
    return 'tab-discard-exception-add-dialog';
  }

  static get template() {
    return getTemplate();
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    this.$.dialog.close();
    this.$.input.submit();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-add-dialog': ExceptionAddDialogElement;
  }
}

customElements.define(
    ExceptionAddDialogElement.is,
    ExceptionAddDialogElement);
