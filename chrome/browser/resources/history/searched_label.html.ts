// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {HistorySearchedLabelElement} from './searched_label.js';

export function getHtml(this: HistorySearchedLabelElement) {
  return html`<slot></slot>`;
}
