// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
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
import {CloseReason, ComposeDialogCallbackRouter, ComposeResponse, ComposeStatus, Length, Tone} from './compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from './compose_api_proxy.js';
import {ComposeTextareaElement} from './textarea.js';

// Struct with ComposeAppElement's properties that need to be saved to return
// the element to a specific state.
interface ComposeAppState {
  input: string;
}

export interface ComposeAppElement {
  $: {
    body: HTMLElement,
    closeButton: HTMLElement,
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
      input_: {
        type: String,
        observer: 'onInputChanged_',
      },
      isSubmitEnabled_: {
        type: Boolean,
        value: false,
      },
      loading_: {
        type: Boolean,
        value: false,
      },
      response_: {
        type: Object,
        value: null,
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

  static get observers() {
    return [
      'saveComposeAppState_(input_)',
    ];
  }

  private apiProxy_: ComposeApiProxy = ComposeApiProxyImpl.getInstance();
  private router_: ComposeDialogCallbackRouter = this.apiProxy_.getRouter();
  private input_: string;
  private isSubmitEnabled_: boolean;
  private loading_: boolean;
  private response_: ComposeResponse|undefined;
  private selectedLength_: Length;
  private selectedTone_: Tone;
  private submitted_: boolean;
  private undoEnabled_: boolean;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.getInitialState_();
    this.router_.responseReceived.addListener((response: ComposeResponse) => {
      this.composeResponseReceived_(response);
    });
  }

  private getInitialState_() {
    this.apiProxy_.requestInitialState().then(initialState => {
      const composeState = initialState.composeState;
      this.loading_ = composeState.hasPendingRequest;
      this.submitted_ =
          composeState.hasPendingRequest || Boolean(composeState.response);
      this.response_ = composeState.response;

      if (composeState.webuiState) {
        const appState: ComposeAppState = JSON.parse(composeState.webuiState);
        this.input_ = appState.input;
      }
    });
  }

  private onClose_() {
    this.apiProxy_.closeUi(CloseReason.kCloseButton);
  }

  private onSubmit_() {
    if (!this.$.textarea.validate()) {
      return;
    }

    this.submitted_ = true;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
    this.compose_();
  }

  private onAccept_() {
    this.apiProxy_.acceptComposeResult();
  }

  private onInputChanged_() {
    this.isSubmitEnabled_ = this.$.textarea.validate();
  }

  private compose_() {
    this.loading_ = true;
    this.response_ = undefined;
    this.apiProxy_.compose(
        {
          length: this.selectedLength_,
          tone: this.selectedTone_,
        },
        this.input_);
  }

  private composeResponseReceived_(response: ComposeResponse) {
    this.response_ = response;
    this.loading_ = false;
    this.undoEnabled_ = response.undoAvailable;
    this.requestUpdateScroll();
  }

  private hasSuccessfulResponse_(): boolean {
    return this.response_?.status === ComposeStatus.kOk;
  }

  private hasFailedResponse_(): boolean {
    return this.response_?.status === ComposeStatus.kError;
  }

  private saveComposeAppState_() {
    const state: ComposeAppState = {input: this.input_};
    // TODO(johntlee): Throttle this call or parts of this call (eg. input).
    this.apiProxy_.saveWebuiState(JSON.stringify(state));
  }

  private async onUndoClick_() {
    try {
      const state = await this.apiProxy_.undo();
      if (state == null) {
        // Attempted to undo when there are no compose states available to undo.
        // Ensure undo is disabled since it is not possible.
        this.undoEnabled_ = false;
        return;
      }
      // Restore state to the state returned by Undo.
      this.response_ = state!.response!;
      this.undoEnabled_ = state!.response!.undoAvailable;
      this.selectedLength_ = state!.style.length;
      this.selectedTone_ = state!.style.tone;
    } catch (error) {
      // Error (e.g., disconnected mojo pipe) from a rejected Promise.
      // Previously, we received a true `undo_available` field in either
      // RequestInitialState(), ComposeResponseReceived(), or a previous Undo().
      // So we think it is possible to undo, but the Promise failed.
      // Allow the user to try again. Leave the undo button enabled.
      // TODO(b/301368162) Ask UX how to handle the edge case of multiple fails.
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-app': ComposeAppElement;
  }
}

customElements.define(ComposeAppElement.is, ComposeAppElement);
