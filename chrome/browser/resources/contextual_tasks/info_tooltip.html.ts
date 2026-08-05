// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksInfoTooltipElement} from './info_tooltip.js';

export function getHtml(this: ContextualTasksInfoTooltipElement) {
  return html`<!--_html_template_start_-->
    <cr-tooltip id="tooltip" exportparts="tooltip" role="dialog"
      position="top"
      offset="0"
      manual-mode>
      <div id="tooltipContent">
        <div class="tooltip-header">
          ${
      this.titleText ?
          html`<div class="tooltip-title">${this.titleText}</div>` :
          ''}
        </div>
        <div class="tooltip-body">${this.bodyText}</div>
        ${
      this.closeButtonType === 'icon' ? html`
          <cr-icon-button id="closeBtn" iron-icon="cr:clear"
              aria-label="$i18n{close}" @click="${this.onTooltipCloseClick_}">
          </cr-icon-button>
        ` : ''}
      </div>
      ${
      this.closeButtonType === 'text' ? html`
        <div id="buttons">
          <cr-button class="action-button" @click="${
                                            this.onTooltipCloseClick_}">
            ${this.buttonText}
          </cr-button>
        </div>
      ` : ''}
    </cr-tooltip>
  <!--_html_template_end_-->`;
}
