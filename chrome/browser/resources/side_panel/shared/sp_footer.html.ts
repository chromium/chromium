// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpFooterElement} from './sp_footer.js';

export function getHtml(this: SpFooterElement) {
  return html`<slot></slot>`;
}
