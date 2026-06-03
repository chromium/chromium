// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './ai_taskbox.html.js';

export class AiTaskboxElement extends CrLitElement {
  static get is() {
    return 'ai-taskbox';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-taskbox': AiTaskboxElement;
  }
}

customElements.define(AiTaskboxElement.is, AiTaskboxElement);
