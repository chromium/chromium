// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerDocumentOutlineElement} from './viewer_document_outline.js';

export function getHtml(this: ViewerDocumentOutlineElement) {
  // clang-format off
  return this.bookmarks.map(item =>
      html`<viewer-bookmark .bookmark="${item}" depth="0"></viewer-bookmark>`);
  // clang-format on
}
