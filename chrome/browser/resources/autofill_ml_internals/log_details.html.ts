// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LogDetailsElement} from './log_details.js';

export function getHtml(this: LogDetailsElement) {
  // Placeholder element. Will be implemented in a follow-up.
  return html`
<h2>Details</h2>
<div>Selected log: ${this.log.formSignature}</div>`;
}
