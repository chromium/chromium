// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageActionInternalsAppElement} from './app.js';

export function getHtml(this: PageActionInternalsAppElement) {
  return html`
<h1>Page Action Internals</h1>`;
}
