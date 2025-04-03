// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RequestElement} from './request.js';

export function getHtml(this: RequestElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-expand-button class="cr-row first" ?expanded="${this.expanded_}"
    @expanded-changed="${this.onExpandedChanged_}">
  <span class="label">${this.getTimestamp_()}</span>
  <cr-icon class="label status" icon="${this.getStatusIcon_()}"
      title="${this.getStatusTitle_()}">
  </cr-icon>
  <span class="label">${this.getRequestPath_()}</span>
  ${this.getPageClassificationLabel_() ? html`
    <cr-chip class="pagelabel" @click="${this.onChipClick_}">
      ${this.getPageClassificationLabel_()}
    </cr-chip>
  ` : ''}
</cr-expand-button>
<cr-collapse ?opened="${this.expanded_}">
  <div class="cr-row" ?hidden="${!this.requestDataJson_}">
    <div class="content"
        .innerHTML="${this.getRequestDataHtml_()}">
    </div>
    <div class="actions">
      <cr-icon-button class="icon-copy-content" title="copy to clipboard"
          @click="${this.onCopyRequestClick_}">
      </cr-icon-button>
    </div>
  </div>
  <div class="cr-row" ?hidden="${!this.responseJson_}">
    <div class="content" .innerHTML="${this.getResponseHtml_()}"></div>
    <div class="actions">
      <cr-icon-button class="icon-copy-content" title="copy to clipboard"
          @click="${this.onCopyResponseClick_}">
      </cr-icon-button>
      <cr-icon-button iron-icon="suggest:lock" title="hardcode response"
          @click="${this.onHardcodeResponseClick_}">
      </cr-icon-button>
    </div>
  </div>
</cr-collapse>
<!--_html_template_end_-->`;
  // clang-format on
}
