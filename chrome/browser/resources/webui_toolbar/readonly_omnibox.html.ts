// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReadonlyOmniboxElement} from './readonly_omnibox.js';

export function getHtml(this: ReadonlyOmniboxElement) {
  // clang-format off
  // This avoids any whitespace text nodes floating around that can confuse
  // things.
  return html`<!--_html_template_start_-->
<div contenteditable id="textContainer">${this.omniboxViewState.text}</div>
<!--_html_template_end_-->`;
  // clang-format on
}
