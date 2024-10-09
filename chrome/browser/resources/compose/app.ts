// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './strings.m.js';
import './textarea.js';
import './result_text.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/md_select.css.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrA11yAnnouncerElement} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrChipElement} from '//resources/cr_elements/cr_chip/cr_chip.js';
import type {CrFeedbackButtonsElement} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrScrollObserverMixin} from '//resources/cr_elements/cr_scroll_observer_mixin.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {isMac} from '//resources/js/platform.js';
import {Debouncer, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComposeAppAnimator} from './animations/app_animator.js';
import {getTemplate} from './app.html.js';
import type {ComposeResponse, ComposeState, ComposeUntrustedDialogCallbackRouter, ConfigurableParams, PartialComposeResponse} from './compose.mojom-webui.js';
import {CloseReason, InputMode, StyleModifier, UserFeedback} from './compose.mojom-webui.js';
import type {ComposeApiProxy} from './compose_api_proxy.js';
import {ComposeApiProxyImpl} from './compose_api_proxy.js';
import {ComposeStatus} from './compose_enums.mojom-webui.js';
import type {ComposeResultTextElement, TextInput} from './result_text.js';
import type {ComposeTextareaElement} from './textarea.js';

// Struct with ComposeAppElement's properties that need to be saved to return
// the element to a specific state.
export interface ComposeAppState {
  editedInput?: string;
  input: string;
  inputMode: InputMode;
  isEditingSubmittedInput?: boolean;
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
    errorGoBackButton: CrButtonElement,
    acceptButton: CrButtonElement,
    loading: HTMLElement,
    undoButton: CrButtonElement,
    redoButton: CrButtonElement,
    resultContainer: HTMLElement,
    resultTextContainer: HTMLElement,
    resultFooter: HTMLElement,
    submitButton: CrButtonElement,
    submitEditButton: CrButtonElement,
    submitFooter: HTMLElement,
    onDeviceUsedFooter: HTMLElement,
    textarea: ComposeTextareaElement,
    modifierMenu: HTMLSelectElement,
    resultText: ComposeResultTextElement,
    feedbackButtons: CrFeedbackButtonsElement,
    polishChip: CrChipElement,
    elaborateChip: CrChipElement,
    formalizeChip: CrChipElement,
  };
}

/**
 * Delay required for screen readers to read out consecutive messages while
 * focus is being moved between elements.
 */
export const TIMEOUT_MS: number = 700;

const ComposeAppElementBase = I18nMixin(CrScrollObserverMixin(PolymerElement));

// Enumerates trigger points of compose or regenerate calls.
// Used to mark where a compose call was made so focus
// can be restored to the respective element afterwards.
enum TriggerElement {
  SUBMIT_INPUT,  // For initial input or editing input.
  MODIFIER,
  UNDO,
  REDO,
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
      enableUpfrontInputModes: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableUpfrontInputModes'),
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
      isEditingResultText_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
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
        computed: 'isLoadingIndicatorShown_(loading_, hasOutput_)',
      },
      response_: {
        type: Object,
        value: null,
      },
      partialResponse_: {
        type: Object,
        value: null,
      },
      showInputModes_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'shouldShowInputModes_(submitted_, enableUpfrontInputModes)',
      },
      selectedInputMode_: {
        type: Number,
        value: InputMode.kUnset,
      },
      polishChipIcon_: {
        type: String,
        value: 'cr:check',
        reflectToAttribute: true,
      },
      elaborateChipIcon_: {
        type: String,
        value: 'compose:elaborate',
        reflectToAttribute: true,
      },
      formalizeChipIcon_: {
        type: String,
        value: 'compose:formalize',
        reflectToAttribute: true,
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
      redoEnabled_: {
        type: Boolean,
        value: false,
      },
      feedbackEnabled_: {
        type: Boolean,
        value: true,
      },
      responseText_: {
        type: String,
        computed: 'getResponseText_(response_, partialResponse_)',
      },
      outputComplete_: {
        type: Boolean,
      },
      hasOutput_: {
        type: Boolean,
      },
      displayedText_: {
        type: String,
      },
      modifierOptions_: {
        type: Array,
        value: () => {
          return [
            {
              value: StyleModifier.kUnset,
              label: loadTimeData.getString('modifierMenuTitle'),
              isDefault: true,
            },
            {
              value: StyleModifier.kLonger,
              label: loadTimeData.getString('longerOption'),
            },
            {
              value: StyleModifier.kShorter,
              label: loadTimeData.getString('shorterOption'),
            },
            {
              value: StyleModifier.kFormal,
              label: loadTimeData.getString('formalToneOption'),
            },
            {
              value: StyleModifier.kCasual,
              label: loadTimeData.getString('casualToneOption'),
            },
            {
              value: StyleModifier.kRetry,
              label: loadTimeData.getString('retryOption'),
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
      'debounceUpdateResultComplete_(outputComplete_, response_)',
    ];
  }

  enableAnimations: boolean;
  enableUpfrontInputModes: boolean;

  private animator_: ComposeAppAnimator;
  private apiProxy_: ComposeApiProxy = ComposeApiProxyImpl.getInstance();
  private eventTracker_: EventTracker = new EventTracker();
  private router_: ComposeUntrustedDialogCallbackRouter =
      this.apiProxy_.getRouter();
  private showFirstRunDialog_: boolean;
  private showMainAppDialog_: boolean;
  private showMSBBDialog_: boolean;
  private shouldShowMSBBDialog_: boolean;
  private editedInput_: string;
  private feedbackState_: CrFeedbackOption;
  private input_: string;
  private inputParams_: ConfigurableParams;
  private isEditingSubmittedInput_: boolean;
  private isEditingResultText_: boolean;
  private isEditSubmitEnabled_: boolean;
  private isSubmitEnabled_: boolean;
  private loading_: boolean;
  private response_: ComposeResponse|null;
  private partialResponse_: PartialComposeResponse|undefined;
  private saveAppStateDebouncer_: Debouncer;
  private scrollCheckDebouncer_: Debouncer;
  private updateResultCompleteDebouncer_: Debouncer;
  private selectedInputMode_: InputMode;
  private polishChipIcon_: string;
  private elaborateChipIcon_: string;
  private formalizeChipIcon_: string;
  private textSelected_: boolean;
  private submitted_: boolean;
  private undoEnabled_: boolean;
  private redoEnabled_: boolean;
  private feedbackEnabled_: boolean;
  private userHasModifiedState_: boolean = false;
  private lastTriggerElement_: TriggerElement;
  private outputComplete_: boolean = true;
  private hasOutput_: boolean = false;
  private displayedText_: string;
  private responseText_: string;
  private userResponseText_: string|undefined;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.animator_ = new ComposeAppAnimator(
        this, loadTimeData.getBoolean('enableAnimations'));
    this.getInitialState_();
    // If upfront inputs are enabled, the default mode should be set to Polish.
    if (this.enableUpfrontInputModes) {
      this.selectedInputMode_ = InputMode.kPolish;
    }
    this.router_.responseReceived.addListener((response: ComposeResponse) => {
      this.composeResponseReceived_(response);
    });
    this.router_.partialResponseReceived.addListener(
        (partialResponse: PartialComposeResponse) => {
          this.partialComposeResponseReceived_(partialResponse);
        });
  }

  // Overridden from CrScrollObserverMixin in order to change the scrolling
  // container based on the UI Refinements flag.
  override getContainer(): HTMLElement {
    return this.$.resultTextContainer;
  }

  private getResponseText_(): TextInput {
    if (this.userResponseText_ !== undefined) {
      return {
        text: this.userResponseText_,
        isPartial: false,
        streamingEnabled: false,
      };
    } else if (this.response_) {
      return {
        text: this.response_.status === ComposeStatus.kOk ?
            this.response_.result.trim() :
            '',
        isPartial: false,
        streamingEnabled: this.partialResponse_ !== undefined,
      };
    } else if (this.partialResponse_) {
      return {
        text: this.partialResponse_?.result.trim(),
        isPartial: true,
        streamingEnabled: true,
      };
    } else {
      return {text: '', isPartial: false, streamingEnabled: false};
    }
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
      // The dialog can initially be in one of three view states. Completion of
      // the FRE causes the dialog to show the MSBB state if MSBB is not
      // enabled, and the main app state otherwise.
      this.showFirstRunDialog_ = !initialState.freComplete;
      this.showMSBBDialog_ =
          initialState.freComplete && !initialState.msbbState;
      this.shouldShowMSBBDialog_ = !initialState.msbbState;

      this.showMainAppDialog_ =
          initialState.freComplete && initialState.msbbState;

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
        this.redoEnabled_ = Boolean(this.response_?.redoAvailable);
        this.feedbackEnabled_ = Boolean(!this.response_?.providedByUser);
      }

      if (composeState.webuiState) {
        const appState: ComposeAppState = JSON.parse(composeState.webuiState);
        this.input_ = appState.input;
        this.selectedInputMode_ = appState.inputMode;
        this.updateInputMode_();
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
    const fullBodyHeight = this.$.body.offsetHeight;
    const resultContainerHeight = this.$.resultContainer.offsetHeight;
    this.isEditingSubmittedInput_ = false;
    this.$.textarea.focusEditButton();
    this.animator_.transitionFromEditingToResult(resultContainerHeight);
    this.$.textarea.transitionToResult(fullBodyHeight);
    this.$.editTextarea.transitionToResult(fullBodyHeight);

    this.apiProxy_.logCancelEdit();
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

    this.apiProxy_.logEditInput();
  }

  private onIsEditingSubmittedInputChanged_() {
    if (this.isEditingSubmittedInput_) {
      // When switching to editing the submitted input, manually move focus
      // to the input.
      this.$.editTextarea.focusInput();
    }
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

  private onModifierChanged_() {
    const selectedModifier =
      Number(this.$.modifierMenu.value) as StyleModifier;
    this.rewrite_(selectedModifier);
    this.lastTriggerElement_ = TriggerElement.MODIFIER;
    // Immediately clear the selection after triggering a rewrite. A selected
    // index of 0 corresponds to the default value, which is disabled and cannot
    // be selected in the dialog.
    this.$.modifierMenu.selectedIndex = 0;
  }

  private shouldShowInputModes_() {
    return !this.submitted_ && this.enableUpfrontInputModes;
  }

  private onPolishChipClick_() {
    this.selectedInputMode_ = InputMode.kPolish;
    this.updateInputMode_();
  }

  private onElaborateChipClick_() {
    this.selectedInputMode_ = InputMode.kElaborate;
    this.updateInputMode_();
  }

  private onFormalizeChipClick_() {
    this.selectedInputMode_ = InputMode.kFormalize;
    this.updateInputMode_();
  }

  private updateInputMode_() {
    this.userHasModifiedState_ = true;
    this.$.polishChip.selected = this.selectedInputMode_ === InputMode.kPolish;
    this.polishChipIcon_ =
        this.$.polishChip.selected ? 'cr:check' : 'compose:polish';
    this.$.elaborateChip.selected =
        this.selectedInputMode_ === InputMode.kElaborate;
    this.elaborateChipIcon_ =
        this.$.elaborateChip.selected ? 'cr:check' : 'compose:elaborate';
    this.$.formalizeChip.selected =
        this.selectedInputMode_ === InputMode.kFormalize;
    this.formalizeChipIcon_ =
        this.$.formalizeChip.selected ? 'cr:check' : 'compose:formalize';
    this.saveComposeAppState_();
  }

  private openModifierMenuOnKeyDown_(e: KeyboardEvent) {
    // On Windows and Linux, ArrowDown and ArrowUp key events directly change
    // the menu selection, which fires the `select` on-change event without
    // showing what selection was made.
    // MacOS keyboard controls opens the dropdown menu on ArrowUp/Down and thus
    // does not need to override behaviour.
    if (isMac) {
      return;
    }

    // Override keyboard controls for ArrowUp/Down to open the `select` menu.
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      this.$.modifierMenu.showPicker();
    }
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

  private compose_(inputEdited: boolean = false) {
    assert(this.$.textarea.validate());
    assert(this.submitted_);
    // <if expr="is_macosx">
    // For VoiceOver, the screen reader on Mac, to read consecutive alerts the
    // contents must change between announcements. To satisfy this, new results
    // are announced by alternating between this "loading" message and the
    // "updated" message. This is also done to announce updates for the undo
    // and redo functions.
    this.screenReaderAnnounce_(this.i18n('resultLoadingA11yMessage'));
    // </if>
    this.$.body.scrollTop = 0;
    this.loading_ = true;
    this.animator_.transitionInLoading();
    this.userResponseText_ = undefined;
    this.response_ = null;
    this.partialResponse_ = undefined;
    this.feedbackEnabled_ = true;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
    this.apiProxy_.compose(this.input_, this.selectedInputMode_, inputEdited);
  }

  private rewrite_(style: StyleModifier) {
    assert(this.$.textarea.validate());
    assert(this.submitted_);
    // <if expr="is_macosx">
    this.screenReaderAnnounce_(this.i18n('resultLoadingA11yMessage'));
    // </if>
    const bodyHeight = this.$.body.offsetHeight;
    const resultHeight = this.$.resultContainer.offsetHeight;
    this.$.body.scrollTop = 0;
    this.loading_ = true;
    this.userResponseText_ = undefined;
    this.response_ = null;
    this.partialResponse_ = undefined;
    this.feedbackEnabled_ = true;
    this.saveComposeAppState_();  // Ensure state is saved before compose call.
    this.apiProxy_.rewrite(style);
    this.animator_.transitionFromResultToLoading(bodyHeight, resultHeight);
  }

  private debounceUpdateResultComplete_() {
    this.updateResultCompleteDebouncer_ = Debouncer.debounce(
        this.updateResultCompleteDebouncer_, microTask, () => {
          return this.updateResultComplete_();
        });
  }

  private updateResultComplete_() {
    if (!this.response_) {
      return;
    }
    if (this.response_.status === ComposeStatus.kOk) {
      // Don't process OK status until outputComplete_ is true.
      if (!this.outputComplete_) {
        return;
      }
    }

    this.userResponseText_ = undefined;
    const loadingHeight = this.$.loading.offsetHeight;
    this.loading_ = false;
    this.undoEnabled_ = this.response_.undoAvailable;
    this.$.textarea.transitionToEditable();
    if (!this.partialResponse_) {
      if (this.response_.status === ComposeStatus.kOk) {
        this.animator_.transitionFromLoadingToCompleteResult(loadingHeight);
      }
    } else {
      if (this.outputComplete_ && this.response_.status === ComposeStatus.kOk) {
        this.animator_.transitionFromPartialToCompleteResult();
      }
    }

    switch (this.lastTriggerElement_) {
      case TriggerElement.SUBMIT_INPUT:
        this.$.textarea.focusEditButton();
        break;
      case TriggerElement.MODIFIER:
        this.$.modifierMenu.focus({ preventScroll: true });
        break;
      case TriggerElement.UNDO:
        this.$.undoButton.focus();
        break;
      case TriggerElement.REDO:
        this.$.redoButton.focus();
        break;
    }

    this.screenReaderAnnounce_(
        this.i18n('resultUpdatedA11yMessage'), TIMEOUT_MS);
  }

  private composeResponseReceived_(response: ComposeResponse) {
    this.feedbackState_ = CrFeedbackOption.UNSPECIFIED;
    this.response_ = response;
    this.redoEnabled_ = false;
    this.feedbackEnabled_ = true;
  }

  private partialComposeResponseReceived_(partialResponse:
                                              PartialComposeResponse) {
    assert(!this.response_);
    this.feedbackState_ = CrFeedbackOption.UNSPECIFIED;
    this.partialResponse_ = partialResponse;
  }

  private isLoadingIndicatorShown_(): boolean {
    return this.loading_ && !this.hasOutput_;
  }

  // Elements related to results should be hidden when the output is empty, but
  // not if the results are in an edited state. The latter corresponds with
  // feedback being disabled.
  private hideResults_(): boolean {
    return !this.hasOutput_ && this.feedbackEnabled_;
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
      case ComposeStatus.kRequestTimeout:
        return this.i18n('errorTryAgainLater');
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

  private isBackFromErrorAvailable_(): boolean {
    // True when the current response is a filtering error and resulted from
    // applying a modifier.
    return Boolean(
        this.response_?.status === ComposeStatus.kFiltered &&
        this.response_?.triggeredFromModifier);
  }

  private onResultEdit_(e: CustomEvent<string>) {
    this.userResponseText_ = e.detail;
    this.apiProxy_.editResult(this.userResponseText_).then(isEdited => {
      if (isEdited) {
        this.undoEnabled_ = true;
        this.redoEnabled_ = false;
        this.feedbackEnabled_ = false;
        this.feedbackState_ = CrFeedbackOption.UNSPECIFIED;
      }
    });
  }

  private onSetResultFocus_(e: CustomEvent<boolean>) {
    this.isEditingResultText_ = e.detail;
  }

  private saveComposeAppState_() {
    if (this.saveAppStateDebouncer_?.isActive()) {
      this.saveAppStateDebouncer_.flush();
      return;
    }

    if (!this.userHasModifiedState_) {
      return;
    }

    const state: ComposeAppState = {
      input: this.input_,
      inputMode: this.selectedInputMode_,
    };
    if (this.isEditingSubmittedInput_) {
      state.isEditingSubmittedInput = this.isEditingSubmittedInput_;
      state.editedInput = this.editedInput_;
    }
    this.apiProxy_.saveWebuiState(JSON.stringify(state));
  }

  private async onUndoClick_() {
    // <if expr="is_macosx">
    this.screenReaderAnnounce_(this.i18n('undoResultA11yMessage'));
    // </if>
    try {
      const state = await this.apiProxy_.undo();
      if (state == null) {
        // Attempted to undo when there are no compose states available to undo.
        // Ensure undo is disabled since it is not possible.
        this.undoEnabled_ = false;
        return;
      }
      this.updateWithNewState_(state);
      // Focus is moved from the undo button to the redo button if undo is
      // disabled in the new state. Otherwise, the undo button always keeps
      // focus.
      if (this.undoEnabled_) {
        this.lastTriggerElement_ = TriggerElement.UNDO;
      } else {
        this.lastTriggerElement_ = TriggerElement.REDO;
      }
    } catch (error) {
      // Error (e.g., disconnected mojo pipe) from a rejected Promise. Allow the
      // user to try again as there should be a valid state to restore.
      // TODO(b/301368162): Ask UX how to handle the edge case of multiple
      // fails.
    }
  }

  private async onErrorGoBackButton_() {
    try {
      const state = await this.apiProxy_.recoverFromErrorState();
      // This button should only be enabled following application of a modifier,
      // which ensures a previous state to revert to.
      assert(state);

      this.updateWithNewState_(state);
    } catch (error) {
      // Error (e.g., disconnected mojo pipe) from a rejected Promise. Allow the
      // user to try again as there should be a valid state to restore.
      // TODO(b/301368162): Ask UX how to handle the edge case of multiple
      // fails.
    }
  }

  private async onRedoClick_() {
    // <if expr="is_macosx">
    this.screenReaderAnnounce_(this.i18n('redoResultA11yMessage'));
    // </if>
    try {
      const state = await this.apiProxy_.redo();
      if (state == null) {
        // Attempted to redo when there are no compose states available to redo.
        // Ensure redo is disabled since it is not possible.
        this.redoEnabled_ = false;
        return;
      }

      this.updateWithNewState_(state);
      // If redo is disabled, then give focus to the undo button by default.
      if (this.redoEnabled_) {
        this.lastTriggerElement_ = TriggerElement.REDO;
      } else {
        this.lastTriggerElement_ = TriggerElement.UNDO;
      }
    } catch (error) {
      // Error (e.g., disconnected mojo pipe) from a rejected Promise. Allow the
      // user to try again as there should be a valid state to restore.
      // TODO(b/301368162): Ask UX how to handle the edge case of multiple
      // fails.
    }
  }

  private updateWithNewState_(state: ComposeState) {
    // Restore the dialog to the given state.
    this.feedbackEnabled_ = !(state.response?.providedByUser);
    this.userResponseText_ =
        this.feedbackEnabled_ ? undefined : state.response?.result;
    this.response_ = state.response;
    this.partialResponse_ = undefined;
    this.undoEnabled_ = Boolean(state.response?.undoAvailable);
    this.redoEnabled_ = Boolean(state.response?.redoAvailable);
    this.feedbackState_ = userFeedbackToFeedbackOption(state.feedback);
    if (state.webuiState) {
      const appState: ComposeAppState = JSON.parse(state.webuiState);
      this.input_ = appState.input;
    }
  }

  private screenReaderAnnounce_(message: string, wait: number = 0) {
    setTimeout(() => {
      const announcer = getAnnouncerInstance() as CrA11yAnnouncerElement;
      announcer.announce(message, wait);
    });
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
