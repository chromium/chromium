// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './memory_bank_chat.css.js';
import {getHtml} from './memory_bank_chat.html.js';

export class MemoryBankChatElement extends CrLitElement {
  static get is() {
    return 'memory-bank-chat';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-bank-chat': MemoryBankChatElement;
  }
}

customElements.define(MemoryBankChatElement.is, MemoryBankChatElement);
