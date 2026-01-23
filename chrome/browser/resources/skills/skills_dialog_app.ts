// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Skill} from './skill.mojom-webui.js';
import {getCss} from './skills_dialog.css.js';
import {getHtml} from './skills_dialog_app.html.js';
import {SkillsDialogBrowserProxyImpl} from './skills_dialog_browser_proxy.js';

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
  };

  protected get isSaveButtonDisabled() {
    return this.skill_.name.length === 0 || this.skill_.prompt.length === 0;
  }

  protected onNameChanged_(e: CustomEvent<{value: string}>) {
    this.skill_ = {...this.skill_, name: e.detail.value};
  }

  protected onInstructionsChanged_(e: CustomEvent<{value: string}>) {
    this.skill_ = {...this.skill_, prompt: e.detail.value};
  }

  /** Submits skill and closes the dialog. */
  protected async submitSkill_(): Promise<void> {
    await SkillsDialogBrowserProxyImpl.getInstance().handler.submitSkill(
        this.skill_);
  }

  /** Click listener for the cancel button. */
  protected cancel_(e: Event) {
    e.preventDefault();
    SkillsDialogBrowserProxyImpl.getInstance().handler.closeDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-dialog-app': SkillsDialogAppElement;
  }
}

customElements.define(SkillsDialogAppElement.is, SkillsDialogAppElement);
