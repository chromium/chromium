// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PerformanceInterventionButtonElement} from './performance_intervention_button.js';

export function getHtml(this: PerformanceInterventionButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-icon-button id="button"
    iron-icon="webui-toolbar:speed"
    ?is-activated="${this.state.isActive}"
    @click="${this.onClick_}"
    @pointerdown="${this.onPointerdown_}"
    title="${this.getTooltip_()}" aria-label="${this.getLabel_()}"
    suppress-rtl-flip>
</cr-icon-button>
<!--_html_template_end_-->`;
  // clang-format on
}
