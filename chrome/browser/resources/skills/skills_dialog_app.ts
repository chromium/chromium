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

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Skill} from './skill.mojom-webui.js';
import {SkillSource} from './skill.mojom-webui.js';
import {getCss} from './skills_dialog.css.js';
import {getHtml} from './skills_dialog_app.html.js';
import {SkillsDialogBrowserProxy} from './skills_dialog_browser_proxy.js';

const DEFAULT_EMOJI: string = '⚡';
export const MAX_PROMPT_CHAR_COUNT = 20000;
const REFINE_SKILL_TIMEOUT_MS = 5000;

let windowProxyInstance: WindowProxy|null = null;

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
    accountEmail: HTMLElement,
    cancelButton: HTMLElement,
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
    refineErrorMessage: HTMLElement,
    saveButton: CrButtonElement,
    saveErrorContainer: HTMLElement,
    textareaWrapper: HTMLElement,
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
      hasRefineError_: {type: Boolean},
      isRefineLoading_: {type: Boolean},
      isAutoGenerationLoading_: {type: Boolean},
      hasSaveError_: {type: Boolean},
    };
  }

  /** The object will be manipulated by submitSkill(). */
  protected accessor skill_: Skill = {
    id: '',
    sourceSkillId: '',
    name: '',
    icon: DEFAULT_EMOJI,
    prompt: '',
    // Default to user created since these are added by the user via the UI.
    source: SkillSource.kUserCreated,
    description: '',
    creationTime: {internalValue: 0n},
    lastUpdateTime: {internalValue: 0n},
  };

  protected accessor dialogTitle_: string = '';
  protected accessor canUndoRefine_: boolean = false;
  protected accessor canRedoRefine_: boolean = false;
  protected accessor shouldShowErrorPage_: boolean =
      !loadTimeData.getBoolean('isGlicEnabled');
  protected accessor signedInEmail_: string = '';
  protected accessor hasRefineError_: boolean = false;
  protected accessor isRefineLoading_: boolean = false;
  protected accessor isAutoGenerationLoading_: boolean = false;
  protected accessor hasSaveError_: boolean = false;

  private originalPrompt_: string = '';
  private refinedPrompt_: string = '';

  private textareaResizeObserver_: ResizeObserver|null = null;
  private dialogResizeObserver_: ResizeObserver|null = null;

  protected get isSaveButtonDisabled() {
    return !this.skill_.name || !this.skill_.prompt ||
        this.skill_.name.length === 0 || this.skill_.prompt.length === 0;
  }

  /** Initializes dialog. */
  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
    SkillsDialogBrowserProxy.getInstance().handler.getInitialSkill().then(
        ({skill}) => {
          if (skill) {
            this.skill_ = skill;
            this.skill_.source = skill.source || SkillSource.kUserCreated;
            // TODO(marissashen): Update to passing in dialogType from dialog
            // creation
            if (!skill.id || skill.source === SkillSource.kFirstParty) {
              // Creating a new skill or remixing a first party skill.
              this.dialogTitle_ = loadTimeData.getString('addSkillHeader');
              this.autoPopulateNameAndIcon_();
            } else {
              // Editing a user created skill.
              this.dialogTitle_ = loadTimeData.getString('editSkillHeader');
            }
          }
        });
    SkillsDialogBrowserProxy.getInstance().handler.getSignedInEmail().then(
        ({email}) => {
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

  override updated(changedProperties: PropertyValues) {
    super.updated(changedProperties as PropertyValues<this>);

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
    const textarea = this.instructionsTextarea_;
    const hasScrollbar = textarea.scrollHeight > textarea.clientHeight;
    const isScrolledToBottom =
        textarea.scrollTop + textarea.clientHeight >= textarea.scrollHeight;
    textarea.classList.toggle(
        'has-overflow', hasScrollbar && !isScrolledToBottom);
  }

  private get instructionsTextarea_(): HTMLTextAreaElement {
    const el =
        this.shadowRoot.querySelector<HTMLTextAreaElement>('#instructionsText');
    assert(el);
    return el;
  }

  protected onEmojiBtnClick_(e: Event) {
    const input = e.target as HTMLInputElement;

    input.focus();
    input.select();

    SkillsDialogBrowserProxy.getInstance().handler.showEmojiPicker();
  }

  protected onEmojiKeydown_(e: KeyboardEvent) {
    if (e.key === 'Tab') {
      return;
    }
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.onEmojiBtnClick_(e);
      return;
    }
    // Block everything else (a-z, 1-9, symbols).
    // This stops the user from manually typing, making it feel "read-only".
    e.preventDefault();
  }

  protected onEmojiInput_(e: Event) {
    const input = e.target as HTMLInputElement;
    const rawValue = input.value;

    if (!rawValue) {
      this.skill_ = {...this.skill_, icon: DEFAULT_EMOJI};
      input.value = DEFAULT_EMOJI;
      return;
    }

    // Sanitize input: Take ONLY the last grapheme cluster
    const segmenter = new Intl.Segmenter('en', {granularity: 'grapheme'});
    const segments = [...segmenter.segment(rawValue)];
    const lastEmoji = segments[segments.length - 1]?.segment || DEFAULT_EMOJI;

    this.skill_ = {...this.skill_, icon: lastEmoji};
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

  protected onNameValueChanged_(e: CustomEvent<{value: string}>) {
    this.skill_ = {...this.skill_, name: e.detail.value};
  }

  protected onInstructionsInput_(e: Event) {
    const target = e.target as HTMLTextAreaElement;
    const newValue = target.value;

    this.canUndoRefine_ = false;
    this.canRedoRefine_ = false;
    this.originalPrompt_ = '';
    this.refinedPrompt_ = '';
    this.hasRefineError_ = false;

    this.skill_ = {...this.skill_, prompt: newValue};

    this.checkTextareaOverflow_();
  }

  protected onUndoClick_() {
    // Undo is only enabled after a successful refinement, at which point
    // originalPrompt_ is guaranteed to be populated.
    this.skill_ = {...this.skill_, prompt: this.originalPrompt_};

    this.canUndoRefine_ = false;
    this.canRedoRefine_ = true;
    this.hasRefineError_ = false;

    this.updateComplete.then(() => {
      this.checkTextareaOverflow_();
      this.instructionsTextarea_.focus();
    });
  }

  protected onRedoClick_() {
    this.skill_ = {...this.skill_, prompt: this.refinedPrompt_};

    this.canUndoRefine_ = true;
    this.canRedoRefine_ = false;
    this.hasRefineError_ = false;

    this.updateComplete.then(() => {
      this.checkTextareaOverflow_();
      this.instructionsTextarea_.focus();
    });
  }

  protected onRefineClick_() {
    if (this.isRefineLoading_) {
      return;
    }
    if (!this.originalPrompt_) {
      this.originalPrompt_ = this.skill_.prompt;
    }
    const skillToRefine = {
      ...this.skill_,
      prompt: this.originalPrompt_,
    };

    this.isRefineLoading_ = true;
    this.hasRefineError_ = false;

    // Race the request against the timeout
    return this.requestRefinedSkillWithTimeout_(skillToRefine)
        .then(({refinedSkill}) => {
          // If the server returned null, do not overwrite the current state.
          if (refinedSkill && !this.hasRefineError_) {
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
          this.hasRefineError_ = true;
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
      prompt: this.skill_.prompt.substring(0, MAX_PROMPT_CHAR_COUNT),

      // If remixing first party skill, set parent and clear ID.
      ...(isFirstParty && {
        id: '',
        sourceSkillId: this.skill_.id,
        source: SkillSource.kDerivedFromFirstParty,
      }),
    };

    SkillsDialogBrowserProxy.getInstance().handler.submitSkill(skill).then(
        ({success}) => {
          this.hasSaveError_ = !success;
        });
  }

  protected onCancelClick_(e: Event) {
    this.cancel_(e);
  }

  protected onClose_(e: Event) {
    this.cancel_(e);
  }

  /** Click listener for the cancel button and closing dialog. */
  private cancel_(e: Event) {
    e.preventDefault();
    SkillsDialogBrowserProxy.getInstance().handler.closeDialog();
  }

  protected autoPopulateNameAndIcon_() {
    if (!this.skill_.prompt || this.skill_.name) {
      return;
    }

    this.isAutoGenerationLoading_ = true;
    return this.requestRefinedSkillWithTimeout_(this.skill_)
        .then(({refinedSkill}) => {
          if (refinedSkill) {
            const newName =
                (!this.skill_.name || this.skill_.name.trim() === '') ?
                refinedSkill.name :
                this.skill_.name;
            const newIcon =
                (this.skill_.icon === DEFAULT_EMOJI && refinedSkill.icon) ?
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
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-dialog-app': SkillsDialogAppElement;
  }
}

customElements.define(SkillsDialogAppElement.is, SkillsDialogAppElement);
