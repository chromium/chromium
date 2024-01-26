// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './strings.m.js';
import './textarea.js';
import '//resources/cr_elements/cr_button/cr_button.js';
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
import {Debouncer, microTask, PolymerElement, timeOut} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComposeAppAnimator} from './animations/app_animator.js';
import {getTemplate} from './app.html.js';
import {CloseReason, ComposeDialogCallbackRouter, ComposeResponse, ComposeStatus, ConfigurableParams, Length, PartialComposeResponse, StyleModifiers, Tone, UserFeedback} from './compose.mojom-webui.js';
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
    firstRunDialog: HTMLElement,
    firstRunFooter: HTMLElement,
    firstRunOkButton: CrButtonElement,
    freMsbbDialog: HTMLElement,
    appDialog: HTMLElement,
    body: HTMLElement,
    bodyAndFooter: HTMLElement,
    cancelEditButton: CrButtonElement,
    closeButton: HTMLElement,
    firstRunCloseButton: HTMLElement,
    closeButtonMSBB: HTMLElement,
    editTextarea: ComposeTextareaElement,
    errorFooter: HTMLElement,
    acceptButton: CrButtonElement,
    loading: HTMLElement,
    undoButton: CrButtonElement,
    refreshButton: HTMLElement,
    resultContainer: HTMLElement,
    partialResultText: HTMLElement,
    submitButton: CrButtonElement,
    submitEditButton: CrButtonElement,
    submitFooter: HTMLElement,
    onDeviceUsedFooter: HTMLElement,
    textarea: ComposeTextareaElement,
    lengthMenu: HTMLSelectElement,
    toneMenu: HTMLSelectElement,
  };
}

const ComposeAppElementBase = I18nMixin(CrScrollableMixin(PolymerElement));

// Enumerates trigger points of compose or regenerate calls.
// Used to mark where a compose call was made so focus
// can be restored to the respective element afterwards.
enum TriggerElement {
  SUBMIT_INPUT,  // For initial input or editing input.
  TONE,
  LENGTH,
  REFRESH
}

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
      enableAnimations: {
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
        reflectToAttribute: true,
      },
      loadingIndicatorShown_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'isLoadingIndicatorShown_(loading_, partialResponse_)',
      },
      response_: {
        type: Object,
        value: null,
      },
      partialResponse_: {
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
      submitted_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
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
              isDefault: true,
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
              isDefault: true,
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

  private animator_: ComposeAppAnimator;
  private apiProxy_: ComposeApiProxy = ComposeApiProxyImpl.getInstance();
  private bodyResizeObserver_: ResizeObserver;
  enableAnimations: boolean;
  private eventTracker_: EventTracker = new EventTracker();
  private router_: ComposeDialogCallbackRouter = this.apiProxy_.getRouter();
  private showFirstRunDialog_: boolean;
  private showMainAppDialog_: boolean;
  private showSavedStateDialog_: boolean;
  private showMSBBDialog_: boolean;
  private shouldShowMSBBDialog_: boolean;
  private editedInput_: string;
  private feedbackState_: CrFeedbackOption;
  private input_: string;
  private inputParams_: ConfigurableParams;
  private isEditingSubmittedInput_: boolean;
  private isEditSubmitEnabled_: boolean;
  private isSubmitEnabled_: boolean;
  private loading_: boolean;
  private response_: ComposeResponse|undefined;
  private partialResponse_: PartialComposeResponse|undefined;
  private saveAppStateDebouncer_: Debouncer;
  private scrollCheckDebouncer_: Debouncer;
  private selectedLength_: Length;
  private selectedTone_: Tone;
  private textSelected_: boolean;
  private submitted_: boolean;
  private undoEnabled_: boolean;
  private userHasModifiedState_: boolean = false;
  private lastTriggerElement_: TriggerElement;
  private savedStateNotificationTimeout_: number;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.animator_ = new ComposeAppAnimator(
        this, loadTimeData.getBoolean('enableAnimations'));
    this.getInitialState_();
    this.router_.responseReceived.addListener((response: ComposeResponse) => {
      this.composeResponseReceived_(response);
    });
    this.router_.partialResponseReceived.addListener(
        (partialResponse: PartialComposeResponse) => {
          this.partialComposeResponseReceived_(partialResponse);
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
    // For detecting when to show the Saved State Notification.
    this.eventTracker_.add(window, 'blur', () => {
      this.onWindowBlur_();
    });
    this.bodyResizeObserver_ = new ResizeObserver(() => {
      this.scrollCheckDebouncer_ = Debouncer.debounce(
          this.scrollCheckDebouncer_, timeOut.after(20), () => {
            this.requestUpdateScroll();
          });
    });
    this.bodyResizeObserver_.observe(this.$.body);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.bodyResizeObserver_.disconnect();
  }

  private debounceSaveComposeAppState_() {
    this.saveAppStateDebouncer_ = Debouncer.debounce(
        this.saveAppStateDebouncer_, microTask,
        () => this.saveComposeAppState_());
  }

  private getInitialState_() {
    this.apiProxy_.requestInitialState().then(initialState => {
      this.inputParams_ = initialState.configurableParams;
      // The dialog can initially be in one of three view states. Completion of
      // the FRE causes the dialog to show the MSBB state if MSBB is not
      // enabled, and the main app state otherwise.
      this.showFirstRunDialog_ = !initialState.freComplete;
      this.showMSBBDialog_ =
          initialState.freComplete && !initialState.msbbState;
      this.shouldShowMSBBDialog_ = !initialState.msbbState;

      this.showMainAppDialog_ =
          initialState.freComplete && initialState.msbbState;
      this.showSavedStateDialog_ = false;

      if (initialState.initialInput) {
        this.input_ = initialState.initialInput;
      }
      this.textSelected_ = initialState.textSelected;
      this.partialResponse_ = undefined;
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

      if (this.showFirstRunDialog_) {
        this.animator_.transitionToFirstRun();
      } else {
        this.animator_.transitionInDialog();
      }

      // Wait for one timeout to flush Polymer tasks, then wait for the next
      // render.
      setTimeout(() => {
        requestAnimationFrame(() => this.apiProxy_.showUi());
      });
    });
  }

  private getTrimmedResult_(): string|undefined {
    return this.response_?.result.trim();
  }

  private getTrimmedPartialResult_(): string|undefined {
    return this.partialResponse_?.result.trim();
  }

  private onFirstRunOkButtonClick_() {
    this.apiProxy_.completeFirstRun();

    if (this.shouldShowMSBBDialog_) {
      this.showMSBBDialog_ = true;
    } else {
      this.showMainAppDialog_ = true;
      this.animator_.transitionToInput();
    }

    this.showFirstRunDialog_ = false;
  }

  private onFirstRunBottomTextClick_(e: Event) {
    e.preventDefault();
    // Embedded links do not work in WebUI so handle in the parent event
    // listener.
    if ((e.target as HTMLElement).tagName === 'A') {
      this.apiProxy_.openComposeLearnMorePage();
    }
  }

  private onCancelEditClick_() {
    this.isEditingSubmittedInput_ = false;
    this.$.textarea.focusEditButton();
  }

  private onClose_(e: Event) {
    switch ((e.target as HTMLElement).id) {
      case 'firstRunCloseButton': {
        this.apiProxy_.closeUi(CloseReason.kFirstRunCloseButton);
        break;
      }
      case 'closeButtonMSBB': {
        this.apiProxy_.closeUi(CloseReason.kMSBBCloseButton);
        break;
      }
      case 'closeButton': {
        this.apiProxy_.closeUi(CloseReason.kCloseButton);
        break;
      }
    }
  }

  private onEditedInputChanged_() {
    this.userHasModifiedState_ = true;
    if (!this.isEditSubmitEnabled_) {
      this.isEditSubmitEnabled_ = this.$.editTextarea.validate();
    }
  }

  private onEditClick_() {
    const fullBodyHeight = this.$.body.offsetHeight;
    const resultContainerHeight = this.$.resultContainer.offsetHeight;
    this.editedInput_ = this.input_;
    this.isEditingSubmittedInput_ = true;
    this.animator_.transitionFromResultToEditing(resultContainerHeight);
    this.$.textarea.transitionToEditing(fullBodyHeight);
    this.$.editTextarea.transitionToEditing(fullBodyHeight);
  }

  private onIsEditingSubmittedInputChanged_() {
    if (this.isEditingSubmittedInput_) {
      // When switching to editing the submitted input, manually move focus
      // to the input.
      this.$.editTextarea.focusInput();
    }
  }

  private onRefresh_() {
    this.rewrite_(/*style=*/ null);
    this.lastTriggerElement_ = TriggerElement.REFRESH;
  }

  private onSubmit_() {
    this.isSubmitEnabled_ = this.$.textarea.validate();
    if (!this.isSubmitEnabled_) {
      this.$.textarea.focusInput();
      return;
    }

    this.$.textarea.scrollInputToTop();
    const bodyHeight = this.$.body.offsetHeight;
    const footerHeight = this.$.submitFooter.offsetHeight;
    this.submitted_ = true;
    this.animator_.transitionOutSubmitFooter(bodyHeight, footerHeight);
    this.$.textarea.transitionToReadonly();
    this.compose_();
    this.lastTriggerElement_ = TriggerElement.SUBMIT_INPUT;
  }

  private onSubmitEdit_() {
    this.isEditSubmitEnabled_ = this.$.editTextarea.validate();
    if (!this.isEditSubmitEnabled_) {
      this.$.editTextarea.focusInput();
      return;
    }

    const bodyHeight = this.$.bodyAndFooter.offsetHeight;
    const editTextareaHeight = this.$.editTextarea.offsetHeight;
    this.isEditingSubmittedInput_ = false;
    this.input_ = this.editedInput_;
    this.selectedLength_ = Length.kUnset;
    this.selectedTone_ = Tone.kUnset;
    this.animator_.transitionFromEditingToLoading(bodyHeight);
    this.$.textarea.transitionToReadonly(editTextareaHeight);
    this.$.editTextarea.transitionToReadonly(editTextareaHeight);
    this.compose_(true);
    this.lastTriggerElement_ = TriggerElement.SUBMIT_INPUT;
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
    this.lastTriggerElement_ = TriggerElement.LENGTH;
  }

  private onToneChanged_() {
    this.selectedTone_ = Number(this.$.toneMenu.value) as Tone;
    this.rewrite_(/*style=*/ {tone: this.selectedTone_});
    this.lastTriggerElement_ = TriggerElement.TONE;
  }

  private onFooterClick_(e: Event) {
    if ((e.target as HTMLElement).tagName !== 'A') {
      // Do nothing if a link is not clicked.
      return;
    }
    e.preventDefault();
    // The "File a bug", "survey", and "sign in" links are embedded into the
    // string. Embedded links do not work in WebUI so handle each click in the
    // parent event listener.
    switch ((e.target as HTMLElement).id) {
      case 'bugLink':
        this.apiProxy_.openBugReportingLink();
        break;
      case 'surveyLink':
        this.apiProxy_.openFeedbackSurveyLink();
        break;
      case 'signInLink':
        this.apiProxy_.openSignInPage();
        break;
      default:
        this.apiProxy_.openComposeLearnMorePage();
    }
  }

  private onMsbbSettingsClick_(e: Event) {
    e.preventDefault();
    // Instruct the browser to open the corresponding settings page.
    this.apiProxy_.openComposeSettings();
  }

  private onWindowBlur_() {
    if (!loadTimeData.getBoolean('enableSavedStateNotification')) {
      return;
    }

    // When pressing tab from the last focusable element on the page, the
    // browser seems to reset focus onto document.body and cause a temporary
    // window blur. Do not show the saved state notification in this case
    // since this allows users to hit tab from the last focusable element
    // to loop focus back to the first focusable element.
    if (document.activeElement === document.body) {
      return;
    }

    // Show Saved State Notification if losing focus from the main app dialog.
    if (this.showMainAppDialog_) {
      this.showMainAppDialog_ = false;
      this.showSavedStateDialog_ = true;

      this.savedStateNotificationTimeout_ = setTimeout(() => {
        this.apiProxy_.closeUi(CloseReason.kLostFocus);
      }, loadTimeData.getInteger('savedStateTimeoutInMilliseconds'));
    }
  }

  private onSavedStateDialogClick_() {
    clearTimeout(this.savedStateNotificationTimeout_);
    this.showMainAppDialog_ = true;
    this.showSavedStateDialog_ = false;
  }

  private compose_(inputEdited: boolean = false) {
    assert(this.$.textarea.validate());
    assert(this.submitted_);
    this.$.body.scrollTop = 0;
    this.loading_ = true;
    this.animator_.transitionInLoading();
    this.response_ = undefined;
    this.partialResponse_ = undefined;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
    this.apiProxy_.compose(this.input_, inputEdited);
  }

  private rewrite_(style: StyleModifiers|null) {
    assert(this.$.textarea.validate());
    assert(this.submitted_);
    const bodyHeight = this.$.body.offsetHeight;
    const resultHeight = this.$.resultContainer.offsetHeight;
    this.$.body.scrollTop = 0;
    this.loading_ = true;
    this.response_ = undefined;
    this.partialResponse_ = undefined;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
    this.apiProxy_.rewrite(style);
    this.animator_.transitionFromResultToLoading(bodyHeight, resultHeight);
  }

  private composeResponseReceived_(response: ComposeResponse) {
    this.response_ = response;
    const loadingHeight = this.$.loading.offsetHeight;
    this.loading_ = false;
    this.undoEnabled_ = response.undoAvailable;
    this.feedbackState_ = CrFeedbackOption.UNSPECIFIED;
    this.$.textarea.transitionToEditable();
    if (this.partialResponse_) {
      this.animator_.transitionFromPartialToCompleteResult();
    } else if (this.hasSuccessfulResponse_()) {
      this.animator_.transitionFromLoadingToCompleteResult(loadingHeight);
    }
    this.partialResponse_ = undefined;

    switch (this.lastTriggerElement_) {
      case TriggerElement.SUBMIT_INPUT:
        this.$.textarea.focusEditButton();
        break;
      case TriggerElement.REFRESH:
        this.$.refreshButton.focus({preventScroll: true});
        break;
      case TriggerElement.LENGTH:
        this.$.lengthMenu.focus({preventScroll: true});
        break;
      case TriggerElement.TONE:
        this.$.toneMenu.focus({preventScroll: true});
    }
  }

  private partialComposeResponseReceived_(partialResponse:
                                              PartialComposeResponse) {
    assert(!this.response_);
    this.partialResponse_ = partialResponse;
  }

  private isLoadingIndicatorShown_(): boolean {
    return this.loading_ && !this.partialResponse_;
  }

  private hasSuccessfulResponse_(): boolean {
    return this.response_?.status === ComposeStatus.kOk;
  }

  private hasPartialResponse_(): boolean {
    return Boolean(this.partialResponse_);
  }


  private hasPartialOrCompleteResponse_(): boolean {
    return Boolean(this.partialResponse_) || this.hasSuccessfulResponse_();
  }

  private hasFailedResponse_(): boolean {
    if (!this.response_) {
      return false;
    }

    return this.response_.status !== ComposeStatus.kOk;
  }

  private hasErrorWithLink_(): boolean {
    return this.hasUnsupportedLanguageResponse_() ||
        this.hasPermissionDeniedResponse_();
  }

  private hasUnsupportedLanguageResponse_(): boolean {
    if (!this.response_) {
      return false;
    }

    return this.response_.status === ComposeStatus.kUnsupportedLanguage;
  }

  private hasPermissionDeniedResponse_(): boolean {
    if (!this.response_) {
      return false;
    }

    return this.response_.status === ComposeStatus.kPermissionDenied;
  }

  private onDeviceEvaluationUsed_(): boolean {
    return Boolean(this.response_?.onDeviceEvaluationUsed);
  }

  private showOnDeviceDogfoodFooter_(): boolean {
    return Boolean(this.response_?.onDeviceEvaluationUsed) &&
        loadTimeData.getBoolean('enableOnDeviceDogfoodFooter');
  }

  private acceptButtonText_(): string {
    return this.textSelected_ ? this.i18n('replaceButton') :
                                this.i18n('insertButton');
  }

  private failedResponseErrorText_(): string {
    switch (this.response_?.status) {
      case ComposeStatus.kFiltered:
        return this.i18n('errorFiltered');
      case ComposeStatus.kRequestThrottled:
        return this.i18n('errorRequestThrottled');
      case ComposeStatus.kOffline:
        return this.i18n('errorOffline');
      case ComposeStatus.kClientError:
      case ComposeStatus.kMisconfiguration:
      case ComposeStatus.kServerError:
      case ComposeStatus.kInvalidRequest:
      case ComposeStatus.kRetryableError:
      case ComposeStatus.kNonRetryableError:
      case ComposeStatus.kDisabled:
      case ComposeStatus.kCancelled:
      case ComposeStatus.kNoResponse:
      default:
        return this.i18n('errorTryAgain');
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
      this.partialResponse_ = undefined;
      this.undoEnabled_ = Boolean(state.response?.undoAvailable);
      this.feedbackState_ = userFeedbackToFeedbackOption(state.feedback);

      if (state.webuiState) {
        const appState: ComposeAppState = JSON.parse(state.webuiState);
        this.input_ = appState.input;
        this.selectedLength_ = appState.selectedLength ?? Length.kUnset;
        this.selectedTone_ = appState.selectedTone ?? Tone.kUnset;
      }
      this.$.undoButton.focus();
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
