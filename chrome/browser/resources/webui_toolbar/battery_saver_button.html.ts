// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BatterySaverButtonElement} from './battery_saver_button.js';

export function getHtml(this: BatterySaverButtonElement) {
  return html`
    <cr-icon-button id="button"
        iron-icon="webui-toolbar:battery_saver_refresh_custom"
        aria-label="${this.getLabel_()}"
        title="${this.getTooltip_()}"
        @click="${this.onClick_}">
    </cr-icon-button>
  `;
}
