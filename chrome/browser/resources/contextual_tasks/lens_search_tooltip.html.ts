// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksLensSearchTooltipElement} from './lens_search_tooltip.js';

export function getHtml(this: ContextualTasksLensSearchTooltipElement) {
  return html`<!--_html_template_start_-->
    <cr-tooltip id="tooltip" role="dialog"
      position="top"
      offset="0"
      manual-mode>
      <div id="tooltipContent">
        <div class="tooltip-header">
          <div class="tooltip-title">$i18n{lensSearchTooltipTitle}</div>
        </div>
        <div>$i18n{lensSearchTooltipBody}</div>
      </div>
      <div id="buttons">
        <cr-button class="action-button" @click="${this.onTooltipCloseClick_}">
          $i18n{lensSearchTooltipAcceptButton}
        </cr-button>
      </div>
    </cr-tooltip>
  <!--_html_template_end_-->`;
}
