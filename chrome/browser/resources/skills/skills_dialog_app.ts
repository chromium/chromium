// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './error_page.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Skill} from './skill.mojom-webui.js';
import {SkillSource} from './skill.mojom-webui.js';
import {getCss} from './skills_dialog.css.js';
import {getHtml} from './skills_dialog_app.html.js';
import {SkillsDialogBrowserProxy} from './skills_dialog_browser_proxy.js';

const DEFAULT_EMOJI: string = '⚡';
const MAX_PROMPT_CHAR_COUNT = 20000;

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
      canUndoRefine_: {type: Boolean},
      canRedoRefine_: {type: Boolean},
      shouldShowErrorPage_: {type: Boolean},
    };
  }

  /** The object will be manipulated by submitSkill(). */
  protected accessor skill_: Skill = {
    id: '',
    name: '',
    icon: DEFAULT_EMOJI,
    prompt: '',
    // Default to user created since these are added by the user via the UI.
    source: SkillSource.kUserCreated,
    creationTime: {internalValue: 0n},
    lastUpdateTime: {internalValue: 0n},
  };

  protected accessor canUndoRefine_: boolean = false;
  protected accessor canRedoRefine_: boolean = false;
  protected accessor shouldShowErrorPage_: boolean =
      !loadTimeData.getBoolean('isGlicEnabled');
  protected get isSaveButtonDisabled() {
    return !this.skill_.name || !this.skill_.prompt ||
        this.skill_.name.length === 0 || this.skill_.prompt.length === 0;
  }
  private originalPrompt_: string = '';
  private refinedPrompt_: string = '';

  /** Initializes dialog. */
  override async connectedCallback() {
    super.connectedCallback();

    const initialSkill =
        (await SkillsDialogBrowserProxy.getInstance().handler.getInitialSkill())
            .skill;
    if (initialSkill) {
      this.skill_ = initialSkill;
      this.skill_.icon = initialSkill.icon || DEFAULT_EMOJI;
    }
  }

  protected onEmojiBtnClick_(e: Event) {
    const input = e.target as HTMLInputElement;

    input.focus();
    input.select();

    SkillsDialogBrowserProxy.getInstance().handler.showEmojiPicker();
  }

  protected onEmojiKeyDown_(e: KeyboardEvent) {
    // Block everything else (a-z, 1-9, symbols).
    // This stops the user from manually typing, making it feel "read-only".
    // NOTE: The OS Emoji Picker bypasses this check, so emojis still get
    // through.
    e.preventDefault();
  }

  protected onEmojiChanged_(e: Event) {
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

  protected get isRefineDisabled_() {
    return !this.skill_.prompt || this.skill_.prompt.length === 0;
  }

  protected onNameChanged_(e: CustomEvent<{value: string}>) {
    this.skill_ = {...this.skill_, name: e.detail.value};
  }

  protected onInstructionsChanged_(e: Event) {
    const target = e.target as HTMLTextAreaElement;
    const newValue = target.value;

    this.canUndoRefine_ = false;
    this.canRedoRefine_ = false;
    this.originalPrompt_ = '';
    this.refinedPrompt_ = '';

    this.skill_ = {...this.skill_, prompt: newValue};
  }

  protected onUndoClick_() {
    this.skill_ = {...this.skill_, prompt: this.originalPrompt_};

    this.canUndoRefine_ = false;
    this.canRedoRefine_ = true;

    this.shadowRoot?.querySelector<HTMLElement>('#instructionsText')?.focus();
  }

  protected onRedoClick_() {
    this.skill_ = {...this.skill_, prompt: this.refinedPrompt_};

    this.canUndoRefine_ = true;
    this.canRedoRefine_ = false;

    this.shadowRoot?.querySelector<HTMLElement>('#instructionsText')?.focus();
  }

  protected onRefineClick_() {
    this.originalPrompt_ = this.skill_.prompt;

    return SkillsDialogBrowserProxy.getInstance()
        .handler.refineSkill(this.skill_)
        .then(({refinedSkill}) => {
          // If the server returned null, do not overwrite the current state.
          if (refinedSkill) {
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
            this.shadowRoot?.querySelector<HTMLElement>('#instructionsText')
                ?.focus();
          }
        });
  }

  protected submitSkill_(): void {
    this.skill_.prompt = this.skill_.prompt.slice(0, MAX_PROMPT_CHAR_COUNT);
    SkillsDialogBrowserProxy.getInstance().handler.submitSkill(this.skill_);
  }

  /** Click listener for the cancel button. */
  protected cancel_(e: Event) {
    e.preventDefault();
    SkillsDialogBrowserProxy.getInstance().handler.closeDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-dialog-app': SkillsDialogAppElement;
  }
}

customElements.define(SkillsDialogAppElement.is, SkillsDialogAppElement);
