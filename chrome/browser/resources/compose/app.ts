// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './strings.m.js';
import './textarea.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/md_select.css.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrScrollableMixin} from '//resources/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {Debouncer, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {CloseReason, ComposeDialogCallbackRouter, ComposeResponse, ComposeStatus, ConfigurableParams, ConsentState, Length, StyleModifiers, Tone, UserFeedback} from './compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from './compose_api_proxy.js';
import {ComposeTextareaElement} from './textarea.js';

// Struct with ComposeAppElement's properties that need to be saved to return
// the element to a specific state.
export interface ComposeAppState {
  editedInput?: string;
  input: string;
  isEditingSubmittedInput?: boolean;
  selectedLength?: Length;
  selectedTone?: Tone;
}

export interface ComposeAppElement {
  $: {
    consentDialog: HTMLElement,
    consentFooter: HTMLElement,
    consentNoThanksButton: CrButtonElement,
    consentYesButton: CrButtonElement,
    disclaimerFooter: HTMLElement,
    disclaimerLetsGoButton: CrButtonElement,
    appDialog: HTMLElement,
    body: HTMLElement,
    cancelEditButton: CrButtonElement,
    closeButton: HTMLElement,
    closeButtonConsent: HTMLElement,
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
      enableAnimations_: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableAnimations'),
        reflectToAttribute: true,
      },
      feedbackState_: {
        type: String,
        value: CrFeedbackOption.UNSPECIFIED,
      },
      input_: {
        type: String,
        observer: 'onInputChanged_',
      },
      isEditingSubmittedInput_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: 'onIsEditingSubmittedInputChanged_',
      },
      isEditSubmitEnabled_: {
        type: Boolean,
        value: true,
      },
      isSubmitEnabled_: {
        type: Boolean,
        value: true,
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
      showMainAppDialog_: {
        type: Boolean,
        value: false,
      },
      showDisclaimerFooter_: {
        type: Boolean,
        value: false,
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
            {
              value: Length.kUnset,
              label: loadTimeData.getString('lengthMenuTitle'),
              hidden: true,
            },
            {
              value: Length.kShorter,
              label: loadTimeData.getString('shorterOption'),
            },
            {
              value: Length.kLonger,
              label: loadTimeData.getString('longerOption'),
            },
          ];
        },
      },
      toneOptions_: {
        type: Array,
        value: () => {
          return [
            {
              value: Tone.kUnset,
              label: loadTimeData.getString('toneMenuTitle'),
              hidden: true,
            },
            {
              value: Tone.kCasual,
              label: loadTimeData.getString('casualToneOption'),
            },
            {
              value: Tone.kFormal,
              label: loadTimeData.getString('formalToneOption'),
            },
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
  private enableAnimations_: boolean;
  private eventTracker_: EventTracker = new EventTracker();
  private router_: ComposeDialogCallbackRouter = this.apiProxy_.getRouter();
  private showMainAppDialog_: boolean;
  private showDisclaimerFooter_: boolean;
  private editedInput_: string;
  private feedbackState_: CrFeedbackOption;
  private input_: string;
  private inputParams_: ConfigurableParams;
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
      this.inputParams_ = initialState.configurableParams;
      // The dialog can initially be in one of three view states. The consent
      // view is shown if consent is not currently granted. If consent was
      // granted but not through Compose use, the disclaimer view is shown.
      // Otherwise, full consent causes the dialog to show at the main app
      // state.
      this.showMainAppDialog_ =
          initialState.consentState === ConsentState.kConsented;
      this.showDisclaimerFooter_ =
          initialState.consentState === ConsentState.kExternalConsented;

      if (initialState.initialInput) {
        this.input_ = initialState.initialInput;
      }
      const composeState = initialState.composeState;
      this.feedbackState_ = userFeedbackToFeedbackOption(composeState.feedback);
      this.loading_ = composeState.hasPendingRequest;
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
        this.selectedLength_ = appState.selectedLength ?? Length.kUnset;
        this.selectedTone_ = appState.selectedTone ?? Tone.kUnset;
        if (appState.isEditingSubmittedInput) {
          this.isEditingSubmittedInput_ = appState.isEditingSubmittedInput;
          this.editedInput_ = appState.editedInput!;
        }
      }
      // Wait for one timeout to flush Polymer tasks, then wait for the next
      // render.
      setTimeout(() => {
        requestAnimationFrame(() => this.apiProxy_.showUi());
      });
    });
  }

  private onConsentNoThanksButtonClick_() {
    this.apiProxy_.closeUi(CloseReason.kPageContentConsentDeclined);
  }

  private onConsentYesButtonClick_() {
    this.apiProxy_.approveConsent();
    this.showMainAppDialog_ = true;
  }

  private onDisclaimerLetsGoButtonClick_() {
    this.apiProxy_.acknowledgeConsentDisclaimer();
    this.showDisclaimerFooter_ = false;
    this.showMainAppDialog_ = true;
  }

  private onCancelEditClick_() {
    this.isEditingSubmittedInput_ = false;
  }

  private onClose_(e: Event) {
    const closeReason = (e.target as HTMLElement).id === 'closeButtonConsent' ?
        CloseReason.kConsentCloseButton :
        CloseReason.kCloseButton;
    this.apiProxy_.closeUi(closeReason);
  }

  private onEditedInputChanged_() {
    this.userHasModifiedState_ = true;
    if (!this.isEditSubmitEnabled_) {
      this.isEditSubmitEnabled_ = this.$.editTextarea.validate();
    }
  }

  private onEditClick_() {
    this.editedInput_ = this.input_;
    this.isEditingSubmittedInput_ = true;
  }

  private onIsEditingSubmittedInputChanged_() {
    if (this.isEditingSubmittedInput_) {
      // When switching to editing the submitted input, manually move focus
      // to the input.
      this.$.editTextarea.focusInput();
    }
  }

  private onRefresh_() {
    this.rewrite_(/*style=*/ {});
  }

  private onSubmit_() {
    this.isSubmitEnabled_ = this.$.textarea.validate();
    if (!this.isSubmitEnabled_) {
      this.$.textarea.focusInput();
      return;
    }

    this.submitted_ = true;
    this.compose_();
  }

  private onSubmitEdit_() {
    this.isEditSubmitEnabled_ = this.$.editTextarea.validate();
    if (!this.isEditSubmitEnabled_) {
      this.$.editTextarea.focusInput();
      return;
    }

    this.isEditingSubmittedInput_ = false;
    this.input_ = this.editedInput_;
    this.selectedLength_ = Length.kUnset;
    this.selectedTone_ = Tone.kUnset;
    this.compose_(true);
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
    if (!this.isSubmitEnabled_) {
      this.isSubmitEnabled_ = this.$.textarea.validate();
    }
  }

  private onLengthChanged_() {
    this.selectedLength_ = Number(this.$.lengthMenu.value) as Length;
    this.rewrite_(/*style=*/ {length: this.selectedLength_});
  }

  private onToneChanged_() {
    this.selectedTone_ = Number(this.$.toneMenu.value) as Tone;
    this.rewrite_(/*style=*/ {tone: this.selectedTone_});
  }

  private onFooterClick_(e: Event) {
    e.preventDefault();
    // The "File a bug" and "survey" links are embedded into the string.
    // Embededd links do not work in WebUI so handle each click in the parent
    // event listener.
    switch ((e.target as HTMLElement).id) {
      case 'bugLink':
        this.apiProxy_.openBugReportingLink();
        break;
      case 'surveyLink':
        this.apiProxy_.openFeedbackSurveyLink();
        break;
    }
  }

  private onConsentTopTextClick_(e: Event) {
    e.preventDefault();
    // The "settings" link is embedded into the string used here as it may need
    // to be localized as part of the sentence. However, such embedded links do
    // not function in WebUI. Handle the event by using this parent event
    // listener to target the link and instruct the browser to open the
    // corresponding settings page.
    if ((e.target as HTMLElement).tagName === 'A') {
      this.apiProxy_.openComposeSettings();
    }
  }

  private compose_(inputEdited: boolean = false) {
    assert(this.$.textarea.validate());
    assert(this.submitted_);
    this.loading_ = true;
    this.response_ = undefined;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
    this.apiProxy_.compose(this.input_, inputEdited);
  }

  private rewrite_(style: StyleModifiers) {
    assert(this.$.textarea.validate());
    assert(this.submitted_);
    this.loading_ = true;
    this.response_ = undefined;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
    this.apiProxy_.rewrite(style);
  }

  private composeResponseReceived_(response: ComposeResponse) {
    this.response_ = response;
    this.loading_ = false;
    this.undoEnabled_ = response.undoAvailable;
    this.feedbackState_ = CrFeedbackOption.UNSPECIFIED;
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
    if (this.selectedLength_ !== Length.kUnset) {
      state.selectedLength = this.selectedLength_;
    }
    if (this.selectedTone_ !== Tone.kUnset) {
      state.selectedTone = this.selectedTone_;
    }
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
      this.feedbackState_ = userFeedbackToFeedbackOption(state.feedback);

      if (state.webuiState) {
        const appState: ComposeAppState = JSON.parse(state.webuiState);
        this.input_ = appState.input;
        this.selectedLength_ = appState.selectedLength ?? Length.kUnset;
        this.selectedTone_ = appState.selectedTone ?? Tone.kUnset;
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

  private onFeedbackSelectedOptionChanged_(
      e: CustomEvent<{value: CrFeedbackOption}>) {
    this.feedbackState_ = e.detail.value;
    switch (e.detail.value) {
      case CrFeedbackOption.UNSPECIFIED:
        this.apiProxy_.setUserFeedback(UserFeedback.kUserFeedbackUnspecified);
        return;
      case CrFeedbackOption.THUMBS_UP:
        this.apiProxy_.setUserFeedback(UserFeedback.kUserFeedbackPositive);
        return;
      case CrFeedbackOption.THUMBS_DOWN:
        this.apiProxy_.setUserFeedback(UserFeedback.kUserFeedbackNegative);
        return;
    }
  }
}

function userFeedbackToFeedbackOption(userFeedback: UserFeedback):
    CrFeedbackOption {
  switch (userFeedback) {
    case UserFeedback.kUserFeedbackUnspecified:
      return CrFeedbackOption.UNSPECIFIED;
    case UserFeedback.kUserFeedbackPositive:
      return CrFeedbackOption.THUMBS_UP;
    case UserFeedback.kUserFeedbackNegative:
      return CrFeedbackOption.THUMBS_DOWN;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-app': ComposeAppElement;
  }
}

customElements.define(ComposeAppElement.is, ComposeAppElement);
