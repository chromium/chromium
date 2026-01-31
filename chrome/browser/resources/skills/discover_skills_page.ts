// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './discover_skills_page.html.js';

export class DiscoverSkillsPageElement extends CrLitElement {
  static get is() {
    return 'discover-skills-page';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'discover-skills-page': DiscoverSkillsPageElement;
  }
}

customElements.define(DiscoverSkillsPageElement.is, DiscoverSkillsPageElement);
