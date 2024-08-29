// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerPageIndicatorElement} from './viewer_page_indicator.js';

export function getHtml(this: ViewerPageIndicatorElement) {
  return html`
<div id="text">${this.getLabel_()}</div>
<div id="triangle-end"></div>`;
}
