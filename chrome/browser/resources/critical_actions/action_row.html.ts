// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CriticalActionRowElement} from './action_row.js';

export function getHtml(this: CriticalActionRowElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<td class="uuid-cell" title="${this.item.criticalActionId}">
  ${this.item.criticalActionId}
</td>
<td title="${this.item.timestampRaw.toString()}">
  ${this.item.timestampStr}
</td>
<td>
  ${this.item.visitId !== 0n ? this.item.visitId.toString() : '-'}
</td>
<td title="${this.item.conversationId}">
  ${this.item.conversationId || '-'}
</td>
<td title="${this.item.actorTaskId}">
  ${this.item.actorTaskId || '-'}
</td>
<td>
  <span class="type-pill ${this.getActionTypeClass_()}">
    ${this.item.actionTypeStr}
  </span>
</td>
<td>
  <span>${this.item.actionSourceStr}</span>
</td>
<td class="url-cell" title="${this.item.url}">
  ${this.item.url ? html`
    <a class="url-link" href="${this.item.url}" target="_blank">
      ${this.item.url}
    </a>` : '-'}
</td>
<td class="metadata-cell">
  ${this.item.metadata ? html`
    <div class="metadata-preview" title="Click to toggle full metadata"
        @click="${this.onToggleMetadataClick_}">
      ${this.isExpanded_ ? '▲ Hide JSON' : this.item.metadata}
    </div>
    ${this.isExpanded_ ? html`
      <pre class="metadata-full">${this.formatJson_()}</pre>` : ''}
  ` : '-'}
</td>
<td>
  <button class="row-delete-btn" title="Delete record"
      @click="${this.onDeleteClick_}">
    Delete
  </button>
</td>
<!--_html_template_end_-->`;
  // clang-format on
}
