// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutofillMlInternalsAppElement} from './app.js';

export function getHtml(this: AutofillMlInternalsAppElement) {
  return html`
<h1>Autofill ML Internals</h1>
<div id="greeting">${this.message_}</div>`;
}
