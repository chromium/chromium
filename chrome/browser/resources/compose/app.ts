// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './strings.m.js';
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
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {Debouncer, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {CloseReason, ComposeDialogCallbackRouter, ComposeResponse, ComposeStatus, Length, Tone} from './compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from './compose_api_proxy.js';
import {ComposeTextareaElement} from './textarea.js';

// Struct with ComposeAppElement's properties that need to be saved to return
// the element to a specific state.
export interface ComposeAppState {
  editedInput?: string;
  input: string;
  isEditingSubmittedInput?: boolean;
}

export interface ComposeAppElement {
  $: {
    body: HTMLElement,
    cancelEditButton: CrButtonElement,
    closeButton: HTMLElement,
    editTextarea: ComposeTextareaElement,
    errorFooter: HTMLElement,
    insertButton: CrButtonElement,
    loading: HTMLElement,
    undoButton: CrButtonElement,
    refreshButton: HTMLElement,
    resultContainer: HTMLElement,
    submitButton: CrButtonElement,
    submitEditButton: CrButtonElement,
    textarea: ComposeTextareaElement,
    lengthMenu: HTMLSelectElement,
    toneMenu: HTMLSelectElement,
  };
}

const ComposeAppElementBase = I18nMixin(CrScrollableMixin(PolymerElement));

export class ComposeAppElement extends ComposeAppElementBase {
  static get is() {
    return 'compose-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      editedInput_: {
        type: String,
        observer: 'onEditedInputChanged_',
      },
      input_: {
        type: String,
        observer: 'onInputChanged_',
      },
      isEditingSubmittedInput_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      isEditSubmitEnabled_: {
        type: Boolean,
        value: false,
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
      lengthOptions_: {
        type: Array,
        value: () => {
          return [
            Length.kUnset,
            Length.kShorter,
            Length.kLonger,
          ];
        },
      },
      toneOptions_: {
        type: Array,
        value: () => {
          return [
            Tone.kUnset,
            Tone.kCasual,
            Tone.kFormal,
          ];
        },
      },
    };
  }

  static get observers() {
    return [
      'debounceSaveComposeAppState_(input_, isEditingSubmittedInput_, ' +
          'editedInput_)',
    ];
  }

  private apiProxy_: ComposeApiProxy = ComposeApiProxyImpl.getInstance();
  private eventTracker_: EventTracker = new EventTracker();
  private router_: ComposeDialogCallbackRouter = this.apiProxy_.getRouter();
  private editedInput_: string;
  private input_: string;
  private isEditingSubmittedInput_: boolean;
  private isEditSubmitEnabled_: boolean;
  private isSubmitEnabled_: boolean;
  private loading_: boolean;
  private response_: ComposeResponse|undefined;
  private saveAppStateDebouncer_: Debouncer;
  private selectedLength_: Length;
  private selectedTone_: Tone;
  private submitted_: boolean;
  private undoEnabled_: boolean;
  private userHasModifiedState_: boolean = false;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.getInitialState_();
    this.router_.responseReceived.addListener((response: ComposeResponse) => {
      this.composeResponseReceived_(response);
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(document, 'visibilitychange', () => {
      if (document.visibilityState !== 'visible') {
        // Ensure app state is saved when hiding the dialog.
        this.saveComposeAppState_();
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  private debounceSaveComposeAppState_() {
    this.saveAppStateDebouncer_ = Debouncer.debounce(
        this.saveAppStateDebouncer_, microTask,
        () => this.saveComposeAppState_());
  }

  private getInitialState_() {
    this.apiProxy_.requestInitialState().then(initialState => {
      if (initialState.initialInput) {
        this.input_ = initialState.initialInput;
      }
      const composeState = initialState.composeState;
      this.loading_ = composeState.hasPendingRequest;
      this.selectedLength_ = composeState.style.length;
      this.selectedTone_ = composeState.style.tone;
      this.submitted_ =
          composeState.hasPendingRequest || Boolean(composeState.response);
      if (!composeState.hasPendingRequest) {
        // If there is a pending request, the existing response is outdated.
        this.response_ = composeState.response;
        this.undoEnabled_ = Boolean(this.response_?.undoAvailable);
      }

      if (composeState.webuiState) {
        const appState: ComposeAppState = JSON.parse(composeState.webuiState);
        this.input_ = appState.input;
        if (appState.isEditingSubmittedInput) {
          this.isEditingSubmittedInput_ = appState.isEditingSubmittedInput;
          this.editedInput_ = appState.editedInput!;
        }
      }
    });
  }

  private onCancelEditClick_() {
    this.isEditingSubmittedInput_ = false;
  }

  private onClose_() {
    this.apiProxy_.closeUi(CloseReason.kCloseButton);
  }

  private onEditedInputChanged_() {
    this.userHasModifiedState_ = true;
    this.isEditSubmitEnabled_ = this.$.editTextarea.validate();
  }

  private onEditClick_() {
    this.editedInput_ = this.input_;
    this.isEditingSubmittedInput_ = true;
  }

  private onSubmit_() {
    if (!this.$.textarea.validate()) {
      return;
    }

    this.submitted_ = true;
    this.compose_();
  }

  private onSubmitEdit_() {
    if (!this.$.editTextarea.validate()) {
      return;
    }

    this.isEditingSubmittedInput_ = false;
    this.input_ = this.editedInput_;
    this.compose_();
  }

  private onAccept_() {
    this.apiProxy_.acceptComposeResult().then((success: boolean) => {
      if (success) {
        this.apiProxy_.closeUi(CloseReason.kInsertButton);
      }
    });
  }

  private onInputChanged_() {
    this.userHasModifiedState_ = true;
    this.isSubmitEnabled_ = this.$.textarea.validate();
  }

  private onLengthChanged_() {
    this.selectedLength_ = Number(this.$.lengthMenu.value) as Length;
    this.onSubmit_();
  }

  private onToneChanged_() {
    this.selectedTone_ = Number(this.$.toneMenu.value) as Tone;
    this.onSubmit_();
  }

  private getLengthOptionLabel_(value: Length): string {
    switch (value) {
      case Length.kUnset:
        return this.i18n('lengthMenuTitle');
      case Length.kShorter:
        return this.i18n('shorterOption');
      case Length.kLonger:
        return this.i18n('longerOption');
    }
  }

  private getToneOptionLabel_(value: Tone): string {
    switch (value) {
      case Tone.kUnset:
        return this.i18n('toneMenuTitle');
      case Tone.kCasual:
        return this.i18n('casualToneOption');
      case Tone.kFormal:
        return this.i18n('formalToneOption');
    }
  }

  private onFileBugClick_(e: Event) {
    e.preventDefault();
    this.apiProxy_.openBugReportingLink();
  }

  private compose_() {
    this.loading_ = true;
    this.response_ = undefined;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
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
    if (!this.response_) {
      return false;
    }

    return this.response_.status !== ComposeStatus.kOk;
  }

  private failedResponseErrorText_(): string {
    switch (this.response_?.status) {
      case ComposeStatus.kNotSuccessful:
        return this.i18n('errorRequestNotSuccessful');
      case ComposeStatus.kTryAgain:
        return this.i18n('errorTryAgain');
      case ComposeStatus.kTryAgainLater:
        return this.i18n('errorTryAgainLater');
      case ComposeStatus.kPermissionDenied:
        return this.i18n('errorPermissionDenied');
      case ComposeStatus.kError:
      case ComposeStatus.kMisconfiguration:
      default:
        return this.i18n('errorGeneric');
    }
  }

  private saveComposeAppState_() {
    if (this.saveAppStateDebouncer_?.isActive()) {
      this.saveAppStateDebouncer_.flush();
      return;
    }

    if (!this.userHasModifiedState_) {
      return;
    }

    const state: ComposeAppState = {input: this.input_};
    if (this.isEditingSubmittedInput_) {
      state.isEditingSubmittedInput = this.isEditingSubmittedInput_;
      state.editedInput = this.editedInput_;
    }
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
      this.response_ = state.response;
      this.undoEnabled_ = Boolean(state.response?.undoAvailable);
      this.selectedLength_ = state.style.length;
      this.selectedTone_ = state.style.tone;

      if (state.webuiState) {
        const appState: ComposeAppState = JSON.parse(state.webuiState);
        this.input_ = appState.input;
      }
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
