// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BrowserSwitchAppElement} from './app.js';

export function getHtml(this: BrowserSwitchAppElement) {
  return html`
<h1>
  <cr-icon icon="cr:domain"></cr-icon>
  ${this.computeTitle_()}
</h1>
<p .innerHTML="${this.computeDescription_()}"></p>`;
}
