// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IndigoTextOverlayElement} from './text_overlay.js';

export function getHtml(this: IndigoTextOverlayElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.currentStep_ === 1 ? html`<div>$i18n{textLayerStep1}</div>` : ''}
${this.currentStep_ === 2 ? html`<div>$i18n{textLayerStep2}</div>` : ''}
${this.currentStep_ === 3 ? html`<div>$i18n{textLayerStep3}</div>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
