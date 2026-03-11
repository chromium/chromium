// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SlimWebviewElement} from './slim_web_view.js';

export function getHtml(this: SlimWebviewElement) {
  return html`<iframe></iframe>`;
}
