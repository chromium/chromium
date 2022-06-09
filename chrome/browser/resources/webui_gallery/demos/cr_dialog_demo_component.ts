// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {getTemplate} from './cr_dialog_demo_component.html.js';

interface CrDialogDemoComponent {
  $: {
    dialog: CrDialogElement,
  };
}

class CrDialogDemoComponent extends PolymerElement {
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

  private autofocusInputs_: Boolean = false;
  private isDialogOpen_: Boolean = false;
  private showHeader_: Boolean = false;
  private showFooter_: Boolean = false;
  private showInputs_: Boolean = false;
  private showScrollingBody_: Boolean = false;
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

customElements.define(CrDialogDemoComponent.is, CrDialogDemoComponent);