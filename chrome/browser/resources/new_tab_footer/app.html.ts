// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NewTabFooterAppElement} from './app.js';

export function getHtml(this: NewTabFooterAppElement) {
  // clang-format off
  return html`
 <!-- TODO(crbug.com/409056431): Remove #example-div once actual elements
      added. This is used as a placeholder. -->
<div id="example-div">${this.message_}</div>`;
}