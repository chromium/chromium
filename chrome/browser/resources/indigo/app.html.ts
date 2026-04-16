// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IndigoImageReplacementAppElement} from './app.js';

export function getHtml(this: IndigoImageReplacementAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h1>$i18n{indigoTitle}</h1>
<!--_html_template_end_-->`;
  // clang-format on
}
