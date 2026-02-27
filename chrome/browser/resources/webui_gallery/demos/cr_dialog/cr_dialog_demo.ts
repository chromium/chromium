// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '../demo.css.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_dialog_demo.css.js';
import {getHtml} from './cr_dialog_demo.html.js';

export interface CrDialogDemoElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class CrDialogDemoElement extends CrLitElement {
  static get is() {
    return 'cr-dialog-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      autofocusInput_: {type: Boolean},
      isDialogOpen_: {type: Boolean},
      noCancel_: {type: Boolean},
      showHeader_: {type: Boolean},
      showFooter_: {type: Boolean},
      showInputs_: {type: Boolean},
      showScrollingBody_: {type: Boolean},
      statusTexts_: {type: Array},
    };
  }

  protected accessor autofocusInput_: boolean = false;
  protected accessor isDialogOpen_: boolean = false;
  protected accessor noCancel_: boolean = false;
  protected accessor showHeader_: boolean = false;
  protected accessor showFooter_: boolean = false;
  protected accessor showInputs_: boolean = false;
  protected accessor showScrollingBody_: boolean = false;
  protected accessor statusTexts_: string[] = [];

  private getDialog_(): CrDialogElement|null {
    return this.shadowRoot.querySelector('cr-dialog');
  }

  protected onOpenDialogClick_() {
    this.isDialogOpen_ = true;
  }

  protected onCancelClick_() {
    const dialog = this.getDialog_();
    if (dialog) {
      dialog.cancel();
    }
  }

  protected onConfirmClick_() {
    const dialog = this.getDialog_();
    if (dialog) {
      dialog.close();
    }
  }

  protected onDialogCrDialogOpen_() {
    this.statusTexts_ =
        ['Dialog was opened and fired a `cr-dialog-open` event.'];
  }

  protected onDialogCancel_() {
    this.statusTexts_.push('Dialog was canceled and fired a `cancel` event.');
    this.requestUpdate();
  }

  protected onDialogClose_() {
    this.isDialogOpen_ = false;
    this.statusTexts_.push('Dialog was closed and fired a `close` event.');
  }

  protected onShowHeaderCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.showHeader_ = e.detail.value;
  }

  protected onShowFooterCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.showFooter_ = e.detail.value;
  }

  protected onShowScrollingBodyCheckedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.showScrollingBody_ = e.detail.value;
  }

  protected onShowInputsCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.showInputs_ = e.detail.value;
  }

  protected onAutofocusInputCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.autofocusInput_ = e.detail.value;
  }

  protected onNoCancelCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.noCancel_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-dialog-demo': CrDialogDemoElement;
  }
}

export const tagName = CrDialogDemoElement.is;

customElements.define(CrDialogDemoElement.is, CrDialogDemoElement);
