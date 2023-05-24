// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './tab_discard_exception_add_input.js';

import {PrefsMixin, PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_discard_exception_add_dialog.html.js';
import {TabDiscardExceptionAddInputElement} from './tab_discard_exception_add_input.js';

export interface TabDiscardExceptionAddDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: TabDiscardExceptionAddInputElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const TabDiscardExceptionAddDialogElementBase = PrefsMixin(PolymerElement) as
    Constructor<PrefsMixinInterface&PolymerElement>;

export class TabDiscardExceptionAddDialogElement extends
    TabDiscardExceptionAddDialogElementBase {
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
    'tab-discard-exception-add-dialog': TabDiscardExceptionAddDialogElement;
  }
}

customElements.define(
    TabDiscardExceptionAddDialogElement.is,
    TabDiscardExceptionAddDialogElement);
