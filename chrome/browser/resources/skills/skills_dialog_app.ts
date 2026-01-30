// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Skill} from './skill.mojom-webui.js';
import {SkillSource} from './skill.mojom-webui.js';
import {getCss} from './skills_dialog.css.js';
import {getHtml} from './skills_dialog_app.html.js';
import {SkillsDialogBrowserProxy} from './skills_dialog_browser_proxy.js';

const DEFAULT_EMOJI: string = '⚡';

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
    };
  }

  /** The object will be manipulated by submitSkill(). */
  protected accessor skill_: Skill = {
    id: '',
    name: '',
    icon: '',
    prompt: '',
    // Default to user created since these are added by the user via the UI.
    source: SkillSource.kUserCreated,
    creationTime: {internalValue: 0n},
    lastUpdateTime: {internalValue: 0n},
  };

  protected get isSaveButtonDisabled() {
    return this.skill_.name.length === 0 || this.skill_.prompt.length === 0;
  }

  protected getEmojiDisplay_(): string {
    return this.skill_.icon || DEFAULT_EMOJI;
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

  protected onNameChanged_(e: CustomEvent<{value: string}>) {
    this.skill_ = {...this.skill_, name: e.detail.value};
  }

  protected onInstructionsChanged_(e: CustomEvent<{value: string}>) {
    this.skill_ = {...this.skill_, prompt: e.detail.value};
  }

  /** Submits skill and closes the dialog. */
  protected submitSkill_(): void {
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
