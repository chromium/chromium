// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '/strings.m.js';
import './textarea.js';
import './result_text.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/cr_scrollable.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/md_select.css.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrChipElement} from '//resources/cr_elements/cr_chip/cr_chip.js';
import type {CrFeedbackButtonsElement} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReachedCase} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {isMac} from '//resources/js/platform.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {ComposeAppAnimator} from './animations/app_animator.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
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

interface ModifierOption {
  value: StyleModifier;
  label: string;
  isDefault?: boolean;
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

const ComposeAppElementBase = I18nMixinLit(CrLitElement);

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

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      editedInput_: {type: String},
      enableAnimations: {
        type: Boolean,
        reflect: true,
      },
      enableUpfrontInputModes: {type: Boolean},
      feedbackState_: {type: String},
      hasPartialOutput_: {type: Boolean},
      input_: {type: String},
      inputParams_: {type: Object},
      isEditingSubmittedInput_: {
        type: Boolean,
        reflect: true,
      },
      isEditingResultText_: {
        type: Boolean,
        reflect: true,
      },
      isEditSubmitEnabled_: {type: Boolean},
      isSubmitEnabled_: {type: Boolean},
      loading_: {
        type: Boolean,
        reflect: true,
      },
      loadingIndicatorShown_: {
        type: Boolean,
        reflect: true,
      },
      response_: {type: Object},
      partialResponse_: {type: Object},
      showInputModes_: {
        type: Boolean,
        reflect: true,
      },
      selectedInputMode_: {type: Number},
      selectedModifier_: {type: String},
      textSelected_: {type: Boolean},
      enterprise_: {type: Boolean},
      showMainAppDialog_: {type: Boolean},
      submitted_: {
        type: Boolean,
        reflect: true,
      },
      undoEnabled_: {type: Boolean},
      redoEnabled_: {type: Boolean},
      feedbackEnabled_: {type: Boolean},
      responseText_: {type: Object},
      showFirstRunDialog_: {type: Boolean},
      showMSBBDialog_: {type: Boolean},
      outputComplete_: {type: Boolean},
      hasOutput_: {type: Boolean},
      displayedText_: {type: String},
      modifierOptions_: {type: Array},
    };
  }

  protected accessor editedInput_: string = '';
  accessor enableAnimations: boolean =
      loadTimeData.getBoolean('enableAnimations');
  accessor enableUpfrontInputModes: boolean =
      loadTimeData.getBoolean('enableUpfrontInputModes');
  protected accessor feedbackState_: CrFeedbackOption =
      CrFeedbackOption.UNSPECIFIED;
  protected accessor hasPartialOutput_: boolean = false;
  protected accessor input_: string = '';
  protected accessor inputParams_: ConfigurableParams|undefined;
  protected accessor isEditingSubmittedInput_: boolean = false;
  protected accessor isEditingResultText_: boolean = false;
  protected accessor isEditSubmitEnabled_: boolean = true;
  protected accessor isSubmitEnabled_: boolean = true;
  protected accessor loading_: boolean = false;
  protected accessor loadingIndicatorShown_: boolean = false;
  protected accessor response_: ComposeResponse|null = null;
  protected accessor partialResponse_: PartialComposeResponse|undefined =
      undefined;
  protected accessor showInputModes_: boolean = false;
  protected accessor selectedInputMode_: InputMode = InputMode.kUnset;
  protected accessor selectedModifier_: string = '';
  protected accessor textSelected_: boolean = false;
  protected accessor enterprise_: boolean =
      loadTimeData.getBoolean('useEnterpriseWithoutLoggingPolicy');
  protected accessor showMainAppDialog_: boolean = false;
  protected accessor submitted_: boolean = false;
  protected accessor undoEnabled_: boolean = false;
  protected accessor redoEnabled_: boolean = false;
  protected accessor feedbackEnabled_: boolean = true;
  protected accessor showFirstRunDialog_: boolean = false;
  protected accessor showMSBBDialog_: boolean = false;
  protected accessor outputComplete_: boolean = true;
  protected accessor hasOutput_: boolean = false;
  protected accessor displayedText_: string = '';
  protected accessor modifierOptions_: ModifierOption[] = [
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
  protected accessor responseText_:
      TextInput = {text: '', isPartial: false, streamingEnabled: false};

  private animator_: ComposeAppAnimator;
  private apiProxy_: ComposeApiProxy = ComposeApiProxyImpl.getInstance();
  private eventTracker_: EventTracker = new EventTracker();
  private pendingSave_: boolean = false;
  private router_: ComposeUntrustedDialogCallbackRouter =
      this.apiProxy_.getRouter();
  private shouldShowMSBBDialog_: boolean = false;
  private userHasModifiedState_: boolean = false;
  private lastTriggerElement_: TriggerElement|null = null;
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties = changedProperties as Map<PropertyKey, any>;

    if (changedPrivateProperties.has('editedInput_') &&
        (this.editedInput_ !== '' ||
         changedPrivateProperties.get('editedInput_') !== undefined)) {
      this.userHasModifiedState_ = true;
    }

    if (changedPrivateProperties.has('input_') &&
        (this.input_ !== '' ||
         changedPrivateProperties.get('input_') !== undefined)) {
      this.userHasModifiedState_ = true;
    }

    if (changedPrivateProperties.has('loading_') ||
        changedPrivateProperties.has('hasOutput_')) {
      this.loadingIndicatorShown_ = this.loading_ && !this.hasOutput_;
    }

    if (changedPrivateProperties.has('submitted_') ||
        changedProperties.has('enableUpfrontInputModes')) {
      this.showInputModes_ = !this.submitted_ && this.enableUpfrontInputModes;
    }

    if (changedPrivateProperties.has('response_') ||
        changedPrivateProperties.has('partialResponse_')) {
      this.responseText_ = this.getResponseText_();
    }

    if (changedPrivateProperties.has('input_') ||
        changedPrivateProperties.has('isEditingSubmittedInput_') ||
        changedPrivateProperties.has('editedInput_')) {
      this.pendingSave_ = true;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties = changedProperties as Map<PropertyKey, any>;

    if (changedPrivateProperties.has('editedInput_')) {
      this.onEditedInputChanged_();
    }

    if (changedPrivateProperties.has('input_')) {
      this.onInputChanged_();
    }

    if (changedPrivateProperties.has('isEditingSubmittedInput_')) {
      this.onIsEditingSubmittedInputChanged_();
    }

    if (changedPrivateProperties.has('input_') ||
        changedPrivateProperties.has('isEditingSubmittedInput_') ||
        changedPrivateProperties.has('editedInput_')) {
      this.pendingSave_ = false;
      this.saveComposeAppState_();
    }

    if (changedPrivateProperties.has('outputComplete_') ||
        changedPrivateProperties.has('response_')) {
      this.updateResultComplete_();
    }
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

      // Wait for one timeout to flush tasks, then wait for the next render.
      setTimeout(() => {
        requestAnimationFrame(() => this.apiProxy_.showUi());
      });
    });
  }

  protected onFirstRunOkButtonClick_() {
    this.apiProxy_.completeFirstRun();

    if (this.shouldShowMSBBDialog_) {
      this.showMSBBDialog_ = true;
    } else {
      this.showMainAppDialog_ = true;
      this.animator_.transitionToInput();
    }

    this.showFirstRunDialog_ = false;
  }

  protected onFirstRunBottomTextClick_(e: Event) {
    e.preventDefault();
    // Embedded links do not work in WebUI so handle in the parent event
    // listener.
    if ((e.target as HTMLElement).tagName === 'A') {
      if (this.enterprise_) {
        this.apiProxy_.openEnterpriseComposeLearnMorePage();
      } else {
        this.apiProxy_.openComposeLearnMorePage();
      }
    }
  }

  protected onCancelEditClick_() {
    const fullBodyHeight = this.$.body.offsetHeight;
    const resultContainerHeight = this.$.resultContainer.offsetHeight;
    this.isEditingSubmittedInput_ = false;
    this.$.textarea.focusEditButton();
    this.animator_.transitionFromEditingToResult(resultContainerHeight);
    this.$.textarea.transitionToResult(fullBodyHeight);
    this.$.editTextarea.transitionToResult(fullBodyHeight);

    this.apiProxy_.logCancelEdit();
  }

  protected onClose_(e: Event) {
    switch ((e.target as HTMLElement).id) {
      case 'firstRunCloseButton':
        this.apiProxy_.closeUi(CloseReason.kFirstRunCloseButton);
        break;
      case 'closeButtonMSBB':
        this.apiProxy_.closeUi(CloseReason.kMSBBCloseButton);
        break;
      case 'closeButton':
        this.apiProxy_.closeUi(CloseReason.kCloseButton);
        break;
      default:
        break;
    }
  }

  private onEditedInputChanged_() {
    if (!this.isEditSubmitEnabled_) {
      this.isEditSubmitEnabled_ = this.$.editTextarea.validate();
    }
  }

  protected onEditClick_() {
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

  protected onSubmit_() {
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

  protected onSubmitEdit_() {
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

  protected onAccept_() {
    this.apiProxy_.acceptComposeResult().then((success: boolean) => {
      if (success) {
        this.apiProxy_.closeUi(CloseReason.kInsertButton);
      }
    });
  }

  private onInputChanged_() {
    if (!this.isSubmitEnabled_) {
      this.isSubmitEnabled_ = this.$.textarea.validate();
    }
  }

  protected onModifierChanged_() {
    const selectedModifier =
      Number(this.$.modifierMenu.value) as StyleModifier;
    this.rewrite_(selectedModifier);
    this.lastTriggerElement_ = TriggerElement.MODIFIER;
    // Immediately clear the selection after triggering a rewrite. A selected
    // index of 0 corresponds to the default value, which is disabled and cannot
    // be selected in the dialog.
    this.$.modifierMenu.selectedIndex = 0;
  }

  protected onPolishChipClick_() {
    this.selectedInputMode_ = InputMode.kPolish;
    this.updateInputMode_();
  }

  protected onElaborateChipClick_() {
    this.selectedInputMode_ = InputMode.kElaborate;
    this.updateInputMode_();
  }

  protected onFormalizeChipClick_() {
    this.selectedInputMode_ = InputMode.kFormalize;
    this.updateInputMode_();
  }

  private updateInputMode_() {
    this.userHasModifiedState_ = true;
    this.saveComposeAppState_();
  }

  protected openModifierMenuOnKeyDown_(e: KeyboardEvent) {
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

  protected onFooterClick_(e: Event) {
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
      case 'enterpriseLearnMore':
        this.apiProxy_.openEnterpriseComposeLearnMorePage();
        break;
      default:
        this.apiProxy_.openComposeLearnMorePage();
    }
  }

  protected onMsbbSettingsClick_(e: Event) {
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

  private async updateResultComplete_() {
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
    await this.updateComplete;

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
      case null:
        break;
      default:
        assertNotReachedCase(this.lastTriggerElement_);
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

  protected hideResults_(): boolean {
    return !this.hasOutput_ && this.feedbackEnabled_;
  }

  protected hasSuccessfulResponse_(): boolean {
    return this.response_?.status === ComposeStatus.kOk;
  }

  protected hasPartialResponse_(): boolean {
    return Boolean(this.partialResponse_);
  }

  protected hasPartialOrCompleteResponse_(): boolean {
    return Boolean(this.partialResponse_) || this.hasSuccessfulResponse_();
  }

  protected hasFailedResponse_(): boolean {
    if (!this.response_) {
      return false;
    }

    return this.response_.status !== ComposeStatus.kOk;
  }

  protected hasErrorWithLink_(): boolean {
    return this.hasUnsupportedLanguageResponse_() ||
        this.hasPermissionDeniedResponse_();
  }

  protected hasUnsupportedLanguageResponse_(): boolean {
    if (!this.response_) {
      return false;
    }

    return this.response_.status === ComposeStatus.kUnsupportedLanguage;
  }

  protected hasPermissionDeniedResponse_(): boolean {
    if (!this.response_) {
      return false;
    }

    return this.response_.status === ComposeStatus.kPermissionDenied;
  }

  protected onDeviceEvaluationUsed_(): boolean {
    return Boolean(this.response_?.onDeviceEvaluationUsed);
  }

  protected showOnDeviceDogfoodFooter_(): boolean {
    return Boolean(this.response_?.onDeviceEvaluationUsed) &&
        loadTimeData.getBoolean('enableOnDeviceDogfoodFooter');
  }

  protected acceptButtonText_(): string {
    return this.textSelected_ ? this.i18n('replaceButton') :
                                this.i18n('insertButton');
  }

  protected failedResponseErrorText_(): string {
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

  protected isBackFromErrorAvailable_(): boolean {
    // True when the current response is a filtering error and resulted from
    // applying a modifier.
    return Boolean(
        this.response_?.status === ComposeStatus.kFiltered &&
        this.response_?.triggeredFromModifier);
  }

  protected onResultEdit_(e: CustomEvent<string>) {
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

  protected onSetResultFocus_(e: CustomEvent<boolean>) {
    this.isEditingResultText_ = e.detail;
  }

  private saveComposeAppState_() {
    if (this.pendingSave_) {
      // Partway through an update cycle where this method will be called in
      // updated().
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

  protected async onUndoClick_() {
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

  protected async onErrorGoBackButton_() {
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

  protected async onRedoClick_() {
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
      const announcer = getAnnouncerInstance();
      announcer.announce(message, wait);
    });
  }

  protected onFeedbackSelectedOptionChanged_(
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
      default:
        assertNotReachedCase(e.detail.value);
    }
  }

  protected onInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.input_ = e.detail.value;
  }

  protected onEditedInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.editedInput_ = e.detail.value;
  }

  protected onOutputCompleteChanged_(e: CustomEvent<{value: boolean}>) {
    this.outputComplete_ = e.detail.value;
  }

  protected onHasOutputChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasOutput_ = e.detail.value;
  }

  protected onHasPartialOutputChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasPartialOutput_ = e.detail.value;
  }

  protected isInputModeSelected_(mode: InputMode): boolean {
    return this.selectedInputMode_ === mode;
  }

  protected getChipIcon_(mode: InputMode): string {
    if (this.isInputModeSelected_(mode)) {
      return 'cr:check';
    }
    switch (mode) {
      case InputMode.kPolish:
        return 'compose:polish';
      case InputMode.kElaborate:
        return 'compose:elaborate';
      case InputMode.kFormalize:
        return 'compose:formalize';
      default:
        return '';
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
    default:
      assertNotReachedCase(userFeedback);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-app': ComposeAppElement;
  }
}

customElements.define(ComposeAppElement.is, ComposeAppElement);
