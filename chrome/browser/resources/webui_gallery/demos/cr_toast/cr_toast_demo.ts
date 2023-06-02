// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '../demo.css.js';

import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {getToastManager} from '//resources/cr_elements/cr_toast/cr_toast_manager.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_toast_demo.html.js';

interface CrToastDemoElement {
  $: {
    toast: CrToastElement,
  };
}

class CrToastDemoElement extends PolymerElement {
  static get is() {
    return 'cr-toast-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      message_: String,
      duration_: Number,
      showDismissButton_: Boolean,
    };
  }

  private duration_: number = 0;
  private message_: string = 'Hello, world';
  private showDismissButton_: boolean = false;

  private onHideToastClick_() {
    this.$.toast.hide();
  }

  private onShowToastManagerClick_() {
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

  private onShowToastClick_() {
    this.$.toast.show();
  }
}

export const tagName = CrToastDemoElement.is;

customElements.define(CrToastDemoElement.is, CrToastDemoElement);
