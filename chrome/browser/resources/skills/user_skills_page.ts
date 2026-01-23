// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './user_skills_page.css.js';
import {getHtml} from './user_skills_page.html.js';


export class UserSkillsPageElement extends CrLitElement {
  static get is() {
    return 'user-skills-page';
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
    'user-skills-page': UserSkillsPageElement;
  }
}

customElements.define(UserSkillsPageElement.is, UserSkillsPageElement);
