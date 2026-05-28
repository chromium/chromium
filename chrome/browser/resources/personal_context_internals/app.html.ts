// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PersonalContextInternalsAppElement} from './app.js';

export function getHtml(this: PersonalContextInternalsAppElement) {
  return html`
    <h1>Personal Context Internals</h1>
    <button @click="${this.onTriggerFirstRunClick_}">
      Show First-Run Notice
    </button>
    <div id="message">${this.message_}</div>
  `;
}
