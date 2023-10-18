// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './textarea.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/md_select.css.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrScrollableMixin} from '//resources/cr_elements/cr_scrollable_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {ComposeDialogCallbackRouter, ComposeResponse, Length, Tone} from './compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from './compose_api_proxy.js';
import {ComposeTextareaElement} from './textarea.js';

export interface ComposeAppElement {
  $: {
    body: HTMLElement,
    insertButton: CrButtonElement,
    loading: HTMLElement,
    refreshButton: HTMLElement,
    resultContainer: HTMLElement,
    submitButton: CrButtonElement,
    textarea: ComposeTextareaElement,
  };
}

const ComposeAppElementBase = CrScrollableMixin(PolymerElement);
export class ComposeAppElement extends ComposeAppElementBase {
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
      loading_: {
        type: Boolean,
        value: false,
      },
      result_: {
        type: String,
        value: '',
      },
      selectedLength_: {
        type: Number,
        value: Length.kUnset,
      },
      selectedTone_: {
        type: Number,
        value: Tone.kUnset,
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
  private router_: ComposeDialogCallbackRouter = this.apiProxy_.getRouter();
  private input_: string;
  private isSubmitEnabled_: boolean;
  private loading_: boolean;
  private result_: string|undefined;
  private selectedLength_: Length;
  private selectedTone_: Tone;
  private submitted_: boolean;
  private undoEnabled_: boolean;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.router_.responseReceived.addListener((response: ComposeResponse) => {
      this.composeResponseReceived_(response);
    });
  }

  private onSubmit_() {
    if (!this.$.textarea.validate()) {
      return;
    }

    this.submitted_ = true;
    this.compose_();
  }

  private onTextareaValueChanged_() {
    this.input_ = this.$.textarea.value;
    this.isSubmitEnabled_ = this.$.textarea.validate();
  }

  private compose_() {
    this.loading_ = true;
    this.result_ = undefined;
    this.apiProxy_.compose(
        {
          length: this.selectedLength_,
          tone: this.selectedTone_,
        },
        this.input_);
  }

  private composeResponseReceived_(response: ComposeResponse) {
    this.result_ = response.result || 'error';
    this.loading_ = false;
    this.requestUpdateScroll();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-app': ComposeAppElement;
  }
}

customElements.define(ComposeAppElement.is, ComposeAppElement);
