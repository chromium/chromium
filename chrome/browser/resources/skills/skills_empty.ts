// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './skills_empty.html.js';

export class SkillsEmptyElement extends CrLitElement {
  static get is() {
    return 'skills-empty';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-empty': SkillsEmptyElement;
  }
}

customElements.define(SkillsEmptyElement.is, SkillsEmptyElement);
