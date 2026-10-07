// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabstripNtbAppElement} from './app.js';

export function getHtml(this: TabstripNtbAppElement) {
  return html`
    <new-tab-button></new-tab-button>
  `;
}
