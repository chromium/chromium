// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SelectableIconButtonElement} from './selectable_icon_button.js';

export function getHtml(this: SelectableIconButtonElement) {
  return html`
    <cr-icon-button id="button" role="radio" iron-icon="${this.icon}"
        tabindex="${this.getButtonTabIndex()}"
        aria-checked="${this.getAriaChecked()}"
        aria-disabled="${this.getAriaDisabled()}"
        aria-label="${this.label}"
        title="${this.label}"
        @keydown="${this.onInputKeydown}">
    </cr-icon-button>`;
}
