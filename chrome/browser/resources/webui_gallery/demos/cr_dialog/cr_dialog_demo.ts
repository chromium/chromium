// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '../demo.css.js';

import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_dialog_demo.html.js';

interface CrDialogDemoElement {
  $: {
    dialog: CrDialogElement,
  };
}

class CrDialogDemoElement extends PolymerElement {
  static get is() {
    return 'cr-dialog-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      autofocusInputs_: Boolean,
      isDialogOpen_: Boolean,
      showHeader_: Boolean,
      showFooter_: Boolean,
      showInputs_: Boolean,
      showScrollingBody_: Boolean,
      statusTexts_: Array,
    };
  }

  private autofocusInputs_: boolean = false;
  private isDialogOpen_: boolean = false;
  private showHeader_: boolean = false;
  private showFooter_: boolean = false;
  private showInputs_: boolean = false;
  private showScrollingBody_: boolean = false;
  private statusTexts_: string[] = [];

  private getDialog_(): CrDialogElement|null {
    return this.shadowRoot!.querySelector('cr-dialog');
  }

  private openDialog_() {
    this.isDialogOpen_ = true;
  }

  private onClickCancel_() {
    const dialog = this.getDialog_();
    if (dialog) {
      dialog.cancel();
    }
  }

  private onClickConfirm_() {
    const dialog = this.getDialog_();
    if (dialog) {
      dialog.close();
    }
  }

  private onOpenDialog_() {
    this.statusTexts_ =
        ['Dialog was opened and fired a `cr-dialog-open` event.'];
  }

  private onCancelDialog_() {
    this.push(
        'statusTexts_', 'Dialog was canceled and fired a `cancel` event.');
  }

  private onCloseDialog_() {
    this.isDialogOpen_ = false;
    this.push('statusTexts_', 'Dialog was closed and fired a `close` event.');
  }
}

export const tagName = CrDialogDemoElement.is;

customElements.define(CrDialogDemoElement.is, CrDialogDemoElement);
