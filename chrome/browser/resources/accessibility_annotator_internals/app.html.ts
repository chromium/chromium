// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AccessibilityAnnotatorInternalsAppElement} from './app.js';

export function getHtml(this: AccessibilityAnnotatorInternalsAppElement) {
  return html`
    <h1>Accessibility Annotators Internals</h1>
    <button @click="${this.onTriggerFirstRunClick_}">
      Show First-Run Info
    </button>
    <div id="message">${this.message_}</div>
  `;
}
