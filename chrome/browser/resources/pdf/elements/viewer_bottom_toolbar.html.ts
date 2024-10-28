// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './ink_brush_selector.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerBottomToolbarElement} from './viewer_bottom_toolbar.js';

export function getHtml(this: ViewerBottomToolbarElement) {
  return html`
    <ink-brush-selector .currentType="${this.currentType}">
    </ink-brush-selector>
    <span id="vertical-separator"></span>
  `;
}
