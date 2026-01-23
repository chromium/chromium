// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './skills_dialog.css.js';
import {getHtml} from './skills_dialog_app.html.js';

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
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-dialog-app': SkillsDialogAppElement;
  }
}

customElements.define(SkillsDialogAppElement.is, SkillsDialogAppElement);
