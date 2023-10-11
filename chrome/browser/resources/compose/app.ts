// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './textarea.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/md_select.css.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {Length, StyleModifiers, Tone} from './compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from './compose_api_proxy.js';
import {ComposeTextareaElement} from './textarea.js';

// Mock code.
function generateRandomText(): string {
  const randomNumber = Math.random() * (200 - 50) + 50;
  const result: string[] = [];
  for (let i = 0; i < randomNumber; i++) {
    result.push('text');
  }
  return result.join(' ');
}

export interface ComposeAppElement {
  $: {
    insertButton: CrButtonElement,
    resultContainer: HTMLElement,
    submitButton: CrButtonElement,
    textarea: ComposeTextareaElement,
  };
}

export class ComposeAppElement extends PolymerElement {
  static get is() {
    return 'compose-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      input_: String,
      isSubmitEnabled_: {
        type: Boolean,
        value: false,
      },
      result_: {
        type: String,
        value: '',
      },
      submitted_: {
        type: Boolean,
        value: false,
      },
      undoEnabled_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private apiProxy_: ComposeApiProxy = ComposeApiProxyImpl.getInstance();
  private input_: string;
  private isSubmitEnabled_: boolean;
  private result_: string;
  private submitted_: boolean;
  private undoEnabled_: boolean;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  private onRefreshClick_() {
    this.result_ = generateRandomText();
  }

  private async onSubmit_() {
    if (!this.$.textarea.validate()) {
      return;
    }

    this.submitted_ = true;
    const styleModifiers:
        StyleModifiers = {tone: Tone.kUnset, length: Length.kUnset};
    const composeResult =
        await this.apiProxy_.compose(styleModifiers, this.input_);

    // TODO(b/302742291) store the error if any as well.
    this.result_ = composeResult.result || 'error';
  }

  private onTextareaValueChanged_() {
    this.input_ = this.$.textarea.value;
    this.isSubmitEnabled_ = this.$.textarea.validate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-app': ComposeAppElement;
  }
}

customElements.define(ComposeAppElement.is, ComposeAppElement);
