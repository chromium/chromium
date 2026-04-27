// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './error_page.js';
import './icons.html.js';
import './skills_emoji_picker.js';

import { ColorChangeUpdater } from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type { CrButtonElement } from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type { CrDialogElement } from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type { CrIconElement } from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import type { CrIconButtonElement } from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type { CrInputElement } from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import { assert } from 'chrome://resources/js/assert.js';
import { loadTimeData } from 'chrome://resources/js/load_time_data.js';
import { CrLitElement } from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type { PropertyValues } from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type { Skill } from './skill.mojom-webui.js';
import { SkillsDialogType, SkillSource } from './skill.mojom-webui.js';
import { SkillsPromptRefinementOutcome } from './skill_metrics.mojom-webui.js';
import { getCss } from './skills_dialog.css.js';
import { getHtml } from './skills_dialog_app.html.js';
import { SkillsDialogBrowserProxy } from './skills_dialog_browser_proxy.js';

const DEFAULT_EMOJI: string = '⚡';
export const MAX_NAME_CHAR_COUNT =
  loadTimeData.getInteger('MAX_NAME_CHAR_COUNT');
export const MAX_PROMPT_CHAR_COUNT =
  loadTimeData.getInteger('MAX_PROMPT_CHAR_COUNT');

// The amount of pixels the user can be from the very bottom of the skills
// dialog before the gradient is removed.
export const BOTTOM_SCROLL_OFFSET_PX = 5;
export const REFINE_SKILL_TIMEOUT_MS = 5000;
export const AUTOCOMPLETE_MIN_CHARS = 20;

let windowProxyInstance: WindowProxy | null = null;

export enum PromptError {
  NONE = 0,
  REFINE = 1,
  CHAR_LIMIT = 2,
}

export interface WindowProxy {
  setTimeout(handler: TimerHandler, timeout?: number): number;
}

export class WindowProxyImpl implements WindowProxy {
  setTimeout(handler: TimerHandler, timeout?: number): number {
    return window.setTimeout(handler, timeout);
  }

  static getInstance(): WindowProxy {
    return windowProxyInstance || (windowProxyInstance = new WindowProxyImpl());
  }

  static setInstance(obj: WindowProxy) {
    windowProxyInstance = obj;
  }
}

export interface SkillsDialogAppElement {
  $: {
    accountInfo: HTMLElement,
    cancelButton: HTMLElement,
    deleteButton: CrButtonElement,
    dialog: CrDialogElement,
    emojiTrigger: HTMLInputElement,
    emojiZeroStateIcon: CrIconElement,
    header: HTMLElement,
    iconRedo: CrIconButtonElement,
    iconRefine: CrIconButtonElement,
    iconUndo: CrIconButtonElement,
    instructionsText: HTMLTextAreaElement,
    nameLoaderContainer: HTMLElement,
    nameText: CrInputElement,
    nameErrorMessage: HTMLElement,
    errorMessage: HTMLElement,
    saveButton: CrButtonElement,
    saveErrorContainer: HTMLElement,
    textareaWrapper: HTMLElement,
    generatedPlaceholder: HTMLElement,
    generatedNameText: HTMLElement,
  };
}

export class SkillsDialogAppElement extends CrLitElement {
  static get is() {
    return 'skills-dialog-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      skill_: {type: Object},
      dialogTitle_: {type: String},
      canUndoRefine_: {type: Boolean},
      canRedoRefine_: {type: Boolean},
      shouldShowErrorPage_: {type: Boolean},
      signedInEmail_: {type: String},
      promptError_: {type: Number},
      isRefineLoading_: {type: Boolean},
      isAutoGenerationLoading_: {type: Boolean},
      hasSaveError_: {type: Boolean},
      hasNameCharLimitError_: {type: Boolean},
      isAddDialog_: {type: Boolean},
      showEmojiPicker_: {type: Boolean},
      generatedName_: {type: String},
      generatedIcon_: {type: String},
      isNameInputFocused_: {type: Boolean},
      hasSeenGeneratedSuggestion_: {type: Boolean},
    };
  }

  /** The object will be manipulated by submitSkill(). */
  protected accessor skill_: Skill = {
    id: '',
    sourceSkillId: '',
    name: '',
    icon: '',
    prompt: '',
    // Default to user created since these are added by the user via the UI.
    source: SkillSource.kUserCreated,
    description: '',
    curatedBy: '',
    imageUrl: '',
    creationTime: {internalValue: 0n},
    lastUpdateTime: {internalValue: 0n},
  };

  protected accessor dialogTitle_: string = '';
  protected accessor hasSaveError_: boolean = false;
  protected accessor hasNameCharLimitError_: boolean = false;
  protected accessor showEmojiPicker_: boolean = false;
  protected accessor canUndoRefine_: boolean = false;
  protected accessor canRedoRefine_: boolean = false;
  protected accessor shouldShowErrorPage_: boolean =
    !loadTimeData.getBoolean('isGlicEnabled');
  protected accessor signedInEmail_: string = '';
  protected accessor promptError_: PromptError = PromptError.NONE;
  protected accessor isRefineLoading_: boolean = false;
  protected accessor isAutoGenerationLoading_: boolean = false;
  protected accessor isAddDialog_: boolean = true;
  protected accessor generatedName_: string = '';
  protected accessor generatedIcon_: string = '';
  protected accessor isNameInputFocused_: boolean = false;
  protected accessor hasSeenGeneratedSuggestion_: boolean = false;

  private originalPrompt_: string = '';
  private refinedPrompt_: string = '';
  private undoCount_: number = 0;
  private redoCount_: number = 0;
  private hasUserRefined_: boolean = false;

  private textareaResizeObserver_: ResizeObserver | null = null;
  private dialogResizeObserver_: ResizeObserver | null = null;

  protected get isSaveButtonDisabled() {
    return !this.skill_.name || !this.skill_.prompt ||
      this.skill_.name.length === 0 || this.skill_.prompt.length === 0;
  }

  protected get isRefinementEnabled_() {
    return loadTimeData.getBoolean('isRefinementEnabled');
  }

  protected hasPromptError_(): boolean {
    return this.promptError_ !== PromptError.NONE;
  }

  /** Initializes dialog. */
  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
    SkillsDialogBrowserProxy.getInstance().handler.getInitialState().then(
      ({ initialDialogState }) => {
        if (initialDialogState) {
          this.skill_ = initialDialogState.skill;
          this.skill_.source =
            initialDialogState.skill.source || SkillSource.kUserCreated;
          switch (initialDialogState.dialogType) {
            case SkillsDialogType.kAdd:
              this.dialogTitle_ = loadTimeData.getString('addSkillHeader');
              this.autoPopulateNameAndIcon_();
              this.isAddDialog_ = true;
              break;
            case SkillsDialogType.kEdit:
              this.dialogTitle_ = loadTimeData.getString('editSkillHeader');
              this.isAddDialog_ = false;
              break;
            default:
              break;
          }
        }
      });
    SkillsDialogBrowserProxy.getInstance().handler.getSignedInEmail().then(
      ({ email }) => {
        this.signedInEmail_ = email;
      });
    if (window.ResizeObserver) {
      this.textareaResizeObserver_ = new ResizeObserver(() => {
        this.checkTextareaOverflow_();
      });

      // Need to explicitly observe the native dialog element because cr-dialog
      // always has size 0x0, so does not trigger child size changes required to
      // expand the dialog.
      this.dialogResizeObserver_ = new ResizeObserver(() => {
        const dialog = this.$.dialog?.getNative();
        if (dialog) {
          document.body.style.height = `${dialog.offsetHeight}px`;
        }
      });
    }
  }

  private disconnectTextareaResizeObserver_() {
    if (this.textareaResizeObserver_) {
      this.textareaResizeObserver_.disconnect();
      this.textareaResizeObserver_ = null;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.disconnectTextareaResizeObserver_();
    if (this.dialogResizeObserver_) {
      this.dialogResizeObserver_.disconnect();
      this.dialogResizeObserver_ = null;
    }
  }

  override firstUpdated() {
    this.attachTextareaResizeObserver_();
  }

  override updated(changedProperties: PropertyValues) {
    super.updated(changedProperties as PropertyValues<this>);

    if (changedProperties.has('skill_')) {
      this.promptError_ = this.skill_.prompt.length >= MAX_PROMPT_CHAR_COUNT ?
        PromptError.CHAR_LIMIT :
        PromptError.NONE;
      this.hasNameCharLimitError_ =
        this.skill_.name.length >= MAX_NAME_CHAR_COUNT;

      // Only check overflow if the textarea is currently in the DOM
      if (!this.isRefineLoading_) {
        this.checkTextareaOverflow_();
      }
    }

    // If the loading state changed, the textarea might have been removed from
    // the DOM.
    if (changedProperties.has('isRefineLoading_') && this.isRefineLoading_) {
      // Wait for the DOM to be fully updated.
      this.updateComplete.then(() => {
        this.disconnectTextareaResizeObserver_();
      });
    }

    if (this.dialogResizeObserver_ && this.$.dialog) {
      this.dialogResizeObserver_.observe(this.$.dialog.getNative());
    }
  }

  private attachTextareaResizeObserver_() {
    const textarea = this.instructionsTextarea_;
    this.disconnectTextareaResizeObserver_();
    this.textareaResizeObserver_ = new ResizeObserver(() => {
      this.checkTextareaOverflow_();
    });
    // Observe the current textarea
    this.textareaResizeObserver_.observe(textarea);
    textarea.onscroll = () => this.checkTextareaOverflow_();
  }

  private checkTextareaOverflow_() {
    // During a loading state, the textarea is removed from the DOM.
    if (this.isRefineLoading_) {
      return;
    }
    const textarea = this.instructionsTextarea_;
    const hasScrollbar = textarea.scrollHeight > textarea.clientHeight;
    // Add a small offset so the user doesn't have to scroll to the absolute
    // bottom
    const isScrolledToBottom =
      textarea.scrollTop + textarea.clientHeight + BOTTOM_SCROLL_OFFSET_PX >=
      textarea.scrollHeight;
    textarea.classList.toggle(
      'has-overflow', hasScrollbar && !isScrolledToBottom);
  }

  private get instructionsTextarea_(): HTMLTextAreaElement {
    const el =
      this.shadowRoot.querySelector<HTMLTextAreaElement>('#instructionsText');
    assert(el);
    return el;
  }

  protected onEmojiBtnClick_() {
    this.showEmojiPicker_ = !this.showEmojiPicker_;
  }

  protected onEmojiSelected_(event: CustomEvent<{ emoji: string }>) {
    this.skill_ = { ...this.skill_, icon: event.detail.emoji };
    this.showEmojiPicker_ = false;
    this.$.emojiTrigger.focus();
  }

  protected onEmojiPickerClose_() {
    this.showEmojiPicker_ = false;
    this.$.emojiTrigger.focus();
  }

  protected onEmojiKeydown_(event: KeyboardEvent) {
    if (event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      this.onEmojiBtnClick_();
      return;
    }
  }

  protected onKeydown_(event: KeyboardEvent) {
    if (event.key === 'Escape' && this.showEmojiPicker_) {
      this.onEmojiPickerClose_();
      event.preventDefault();
      event.stopPropagation();
      return;
    }
  }

  protected onEmojiInput_(event: Event) {
    const input = event.target as HTMLInputElement;
    const rawValue = input.value;

    if (!rawValue) {
      this.skill_ = { ...this.skill_, icon: DEFAULT_EMOJI };
      input.value = DEFAULT_EMOJI;
      return;
    }

    // Sanitize input: Take ONLY the last grapheme cluster
    const segmenter = new Intl.Segmenter('en', { granularity: 'grapheme' });
    const segments = [...segmenter.segment(rawValue)];
    const lastEmoji = segments[segments.length - 1]?.segment || DEFAULT_EMOJI;

    this.skill_ = { ...this.skill_, icon: lastEmoji };
    input.value = lastEmoji;
    input.blur();
  }

  protected isRefineDisabled_(): boolean {
    return !this.skill_.prompt || this.skill_.prompt.length === 0 ||
      this.isRefineLoading_;
  }

  protected isUndoDisabled_(): boolean {
    return !this.canUndoRefine_ || this.isRefineLoading_;
  }

  protected isRedoDisabled_(): boolean {
    return !this.canRedoRefine_ || this.isRefineLoading_;
  }

  protected onNameValueChanged_(e: CustomEvent<{ value: string }>) {
    this.skill_ = { ...this.skill_, name: e.detail.value };
  }

  protected onInstructionsInput_(e: Event) {
    const target = e.target as HTMLTextAreaElement;
    const newValue = target.value;

    this.canUndoRefine_ = false;
    this.canRedoRefine_ = false;
    this.originalPrompt_ = '';
    this.refinedPrompt_ = '';

    this.skill_ = { ...this.skill_, prompt: newValue };

    // Clear generated suggestions as the user changed the prompt manually
    this.generatedName_ = '';
    this.generatedIcon_ = '';
  }

  protected onUndoClick_() {
    this.undoCount_++;
    // Undo is only enabled after a successful refinement, at which point
    // originalPrompt_ is guaranteed to be populated.
    this.skill_ = { ...this.skill_, prompt: this.originalPrompt_ };

    this.canUndoRefine_ = false;
    this.canRedoRefine_ = true;

    this.updateComplete.then(() => {
      this.instructionsTextarea_.focus();
    });
  }

  protected onRedoClick_() {
    this.redoCount_++;
    this.skill_ = { ...this.skill_, prompt: this.refinedPrompt_ };

    this.canUndoRefine_ = true;
    this.canRedoRefine_ = false;

    this.updateComplete.then(() => {
      this.instructionsTextarea_.focus();
    });
  }

  protected onRefineClick_() {
    this.hasUserRefined_ = true;
    if (this.isRefineLoading_) {
      return;
    }
    this.undoCount_ = 0;
    this.redoCount_ = 0;

    if (!this.originalPrompt_) {
      this.originalPrompt_ = this.skill_.prompt;
    }
    const skillToRefine = {
      ...this.skill_,
      prompt: this.originalPrompt_,
    };

    this.isRefineLoading_ = true;

    // Race the request against the timeout
    return this.requestRefinedSkillWithTimeout_(skillToRefine)
      .then(({ refinedSkill }) => {
        // If the server returned null, do not overwrite the current state.
        if (refinedSkill && this.promptError_ !== PromptError.REFINE) {
          // Only update if we have a valid result.
          this.skill_ = {
            ...this.skill_,
            // If the refined prompt is missing or empty, keep the original
            // prompt
            prompt: refinedSkill.prompt || this.skill_.prompt,
          };
          this.refinedPrompt_ = refinedSkill.prompt;
          this.canUndoRefine_ = true;
          this.canRedoRefine_ = false;
        }
      })
      .catch(() => {
        this.promptError_ = PromptError.REFINE;
      })
      .finally(() => {
        this.isRefineLoading_ = false;
        this.updateComplete.then(() => {
          this.attachTextareaResizeObserver_();
          this.checkTextareaOverflow_();
          this.instructionsTextarea_.focus();
        });
      });
  }

  /** Submits skill and closes the dialog. */
  protected onSubmitSkillClick_() {
    this.hasSaveError_ = false;
    const isFirstParty = this.skill_.source === SkillSource.kFirstParty;
    const skill = {
      ...this.skill_,
      icon: this.skill_.icon || DEFAULT_EMOJI,
      name: this.skill_.name.substring(0, MAX_NAME_CHAR_COUNT),
      prompt: this.skill_.prompt.substring(0, MAX_PROMPT_CHAR_COUNT),

      // If remixing first party skill, set parent and clear ID.
      ...(isFirstParty && {
        id: '',
        sourceSkillId: this.skill_.id,
        source: SkillSource.kDerivedFromFirstParty,
      }),
    };

    let refinementOutcome = SkillsPromptRefinementOutcome.kNotRefined;
    if (this.hasUserRefined_) {
      if (this.undoCount_ === this.redoCount_) {
        refinementOutcome = SkillsPromptRefinementOutcome.kUsedRefinedPrompt;
      } else if (this.undoCount_ > this.redoCount_) {
        refinementOutcome = SkillsPromptRefinementOutcome.kRevertedAndNotUsed;
      }
    }

    SkillsDialogBrowserProxy.getInstance()
        .handler.submitSkill(skill, refinementOutcome)
        .then(({success}) => {
          this.hasSaveError_ = !success;
        });
  }

  protected onCancelClick_(e: Event) {
    this.cancel_(e);
  }

  protected onClose_(e: Event) {
    this.cancel_(e);
  }

  /** Deletes skill and closes the dialog. */
  protected onDeleteSkillClick_() {
    SkillsDialogBrowserProxy.getInstance().handler.deleteSkill(this.skill_.id);
  }

  /** Click listener for the cancel button and closing dialog. */
  private cancel_(e: Event) {
    e.preventDefault();
    SkillsDialogBrowserProxy.getInstance().handler.closeDialog();
  }

  protected onNameFocus_() {
    this.isNameInputFocused_ = true;
    if (this.generatedName_ || this.generatedIcon_) {
      this.hasSeenGeneratedSuggestion_ = true;
    } else if (
        this.skill_.prompt.length >= AUTOCOMPLETE_MIN_CHARS &&
        !this.skill_.name && !this.skill_.icon &&
        !this.hasSeenGeneratedSuggestion_) {
      this.generateAutocompleteNameAndIcon_();
    }
  }

  protected onNameBlur_() {
    this.isNameInputFocused_ = false;
  }

  protected onNameKeydown_(e: KeyboardEvent) {
    if (e.key === 'Tab' && (this.generatedName_ || this.generatedIcon_)) {
      this.skill_ = {
        ...this.skill_,
        name: this.skill_.name || this.generatedName_,
        icon: this.skill_.icon || this.generatedIcon_,
      };
      this.generatedName_ = '';
      this.generatedIcon_ = '';
      e.preventDefault();
    }
  }

  private generateAutocompleteNameAndIcon_() {
    this.requestGenerateNameAndEmojiWithTimeout_(this.skill_)
        .then(({refinedSkill}) => {
          // Check again that no name was entered during the request
          if (refinedSkill && !this.skill_.name && !this.skill_.icon) {
            this.generatedName_ = refinedSkill.name;
            this.generatedIcon_ = refinedSkill.icon;
            if (this.isNameInputFocused_) {
              this.hasSeenGeneratedSuggestion_ = true;
            }
          }
        })
        .catch(
            () => {
                // Silently fail, do not show error on UI
            });
  }

  protected autoPopulateNameAndIcon_() {
    if (!this.skill_.prompt || this.skill_.name) {
      return;
    }

    this.isAutoGenerationLoading_ = true;
    return this.requestGenerateNameAndEmojiWithTimeout_(this.skill_)
      .then(({ refinedSkill }) => {
        if (refinedSkill) {
          const newName =
            (!this.skill_.name || this.skill_.name.trim() === '') ?
              refinedSkill.name :
              this.skill_.name;
          const newIcon =
            ((this.skill_.icon === DEFAULT_EMOJI || !this.skill_.icon) &&
              refinedSkill.icon) ?
              refinedSkill.icon :
              this.skill_.icon;
          if (newName !== this.skill_.name || newIcon !== this.skill_.icon) {
            this.skill_ = {
              ...this.skill_,
              name: newName || '',
              icon: newIcon || DEFAULT_EMOJI,
            };
          }
        }
      })
      .catch(
        () => {
          // Silently fail for auto-population; do not show error UI to
          // user as this is a background enhancement.
        })
      .finally(() => {
        this.isAutoGenerationLoading_ = false;
      });
  }

  private requestRefinedSkillWithTimeout_(skillToRefine: Skill) {
    const refineRequest =
      SkillsDialogBrowserProxy.getInstance().handler.refineSkill(
        skillToRefine);

    const timeout = new Promise<never>((_, reject) => {
      WindowProxyImpl.getInstance().setTimeout(
        () => reject(new Error('Refine skill timed out')),
        REFINE_SKILL_TIMEOUT_MS);
    });

    return Promise.race([refineRequest, timeout]);
  }

  private requestGenerateNameAndEmojiWithTimeout_(skillToRefine: Skill) {
    if (!loadTimeData.getBoolean('isAutocompleteEnabled')) {
      return Promise.resolve({refinedSkill: null});
    }

    const generateRequest =
      SkillsDialogBrowserProxy.getInstance().handler.generateNameAndEmoji(
        skillToRefine);

    const timeout = new Promise<never>((_, reject) => {
      WindowProxyImpl.getInstance().setTimeout(
        () => reject(new Error('Generate name and emoji timed out')),
        REFINE_SKILL_TIMEOUT_MS);
    });

    return Promise.race([generateRequest, timeout]);
  }

  protected shouldShowGeneratedPlaceholder_(): boolean {
    return this.isNameInputFocused_ && !!this.generatedName_ &&
        !this.skill_.name;
  }

  protected getNamePlaceholder_(): string {
    return this.isNameInputFocused_ && this.generatedName_ ?
        '' :
        loadTimeData.getString('namePlaceholder');
  }

  protected shouldHideEmojiZeroState_(): boolean {
    return !!this.skill_.icon ||
        !!(this.isNameInputFocused_ && this.generatedIcon_);
  }

  protected getEmojiTriggerClass_(): string {
    return this.isNameInputFocused_ && this.generatedIcon_ &&
            !this.skill_.icon ?
        'placeholder-icon' :
        '';
  }

  protected getEmojiTriggerValue_(): string {
    return this.skill_.icon ||
        (this.isNameInputFocused_ ? this.generatedIcon_ : '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-dialog-app': SkillsDialogAppElement;
  }
}

customElements.define(SkillsDialogAppElement.is, SkillsDialogAppElement);
