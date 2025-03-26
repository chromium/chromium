// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toast/cr_toast_manager.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {getToastManager} from '//resources/cr_elements/cr_toast/cr_toast_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_toast_demo.css.js';
import {getHtml} from './cr_toast_demo.html.js';

export interface CrToastDemoElement {
  $: {
    toast: CrToastElement,
  };
}

export class CrToastDemoElement extends CrLitElement {
  static get is() {
    return 'cr-toast-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      message_: {type: String},
      duration_: {type: Number},
      showDismissButton_: {type: Boolean},
    };
  }

  protected accessor duration_: number = 0;
  protected accessor message_: string = 'Hello, world';
  protected accessor showDismissButton_: boolean = false;

  protected onHideToastClick_() {
    this.$.toast.hide();
  }

  protected onShowToastManagerClick_() {
    getToastManager().showForStringPieces([
      {value: '\'', collapsible: false},
      {
        value: 'This is a really really really really long title that should ' +
            'you an idea of some dynamic text that can be truncated.',
        collapsible: true,
      },
      {value: '\' ', collapsible: false},
      {value: 'is a message.', collapsible: false},
    ]);
  }

  protected onShowToastClick_() {
    this.$.toast.show();
  }

  protected onMessageChanged_(e: CustomEvent<{value: string}>) {
    this.message_ = e.detail.value;
  }

  protected onShowDismissButtonChanged_(e: CustomEvent<{value: boolean}>) {
    this.showDismissButton_ = e.detail.value;
  }

  protected onDurationChanged_(e: CustomEvent<{value: number}>) {
    this.duration_ = e.detail.value;
  }
}

export const tagName = CrToastDemoElement.is;

customElements.define(CrToastDemoElement.is, CrToastDemoElement);
