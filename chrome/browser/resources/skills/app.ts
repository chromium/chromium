// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';

export class SkillsAppElement extends CrLitElement {
  static get is() {
    return 'skills-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-app': SkillsAppElement;
  }
}

customElements.define(SkillsAppElement.is, SkillsAppElement);
