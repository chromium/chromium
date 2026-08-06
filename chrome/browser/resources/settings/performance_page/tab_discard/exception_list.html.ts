// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExceptionListElement} from './exception_list.js';

export function getHtml(this: ExceptionListElement) {
  return html`<!--_html_template_start_-->
<div class="cr-row continuation">
  <div class="cr-padded-text">
    $i18n{tabDiscardingExceptionsHeader}
    <div class="secondary">$i18n{tabDiscardingExceptionsDescription}</div>
  </div>
  <cr-button id="addButton" @click="${this.onAddClick_}"
      aria-label="$i18n{tabDiscardingExceptionsAddButtonAriaLabel}">
    $i18n{add}
  </cr-button>
</div>
<div id="noSitesAdded" class="list-frame" ?hidden="${this.hasSites_()}">
  <div class="list-item secondary">$i18n{noSitesAdded}</div>
</div>
<div id="outer" class="list-frame" role="list"
    ?hidden="${!this.hasSites_()}">
  ${this.getSiteList_().map(item => html`
    <tab-discard-exception-entry .entry="${item}"
        role="listitem" @menu-click="${this.onMenuClick_}"
        @show-tooltip="${this.onShowTooltip_}">
    </tab-discard-exception-entry>
  `)}
  <cr-expand-button id="expandButton" no-hover class="hr"
      ?hidden="${!this.hasOverflowSites_()}"
      ?expanded="${this.overflowSiteListExpanded_}"
      @expanded-changed="${this.onOverflowSiteListExpandedChanged_}">
    <div>$i18n{tabDiscardingExceptionsAdditionalSites}</div>
  </cr-expand-button>
  <cr-collapse id="collapse" ?hidden="${!this.hasOverflowSites_()}"
      ?opened="${this.overflowSiteListExpanded_}">
    ${this.getOverflowSiteList_().map(item => html`
      <div class="hr">
        <tab-discard-exception-entry .entry="${item}"
            role="listitem" @menu-click="${this.onMenuClick_}"
            @show-tooltip="${this.onShowTooltip_}">
        </tab-discard-exception-entry>
      </div>
    `)}
  </cr-collapse>
</div>
<cr-tooltip id="tooltip" fit-to-visible-bounds manual-mode position="top">
  ${this.tooltipText_}
</cr-tooltip>
<cr-lazy-render-lit id="menu" .template="${() => html`
  <cr-action-menu role-description="$i18n{menu}">
    <button id="edit" class="dropdown-item" role="menuitem"
        @click="${this.onEditClick_}">
      $i18n{edit}
    </button>
    <button id="delete" class="dropdown-item" role="menuitem"
        @click="${this.onDeleteClick_}">
      $i18n{siteSettingsActionReset}
    </button>
  </cr-action-menu>
`}">
</cr-lazy-render-lit>
${this.showTabbedAddDialog_ ? html`
  <tab-discard-exception-tabbed-add-dialog
      @close="${this.onTabbedAddDialogClose_}">
  </tab-discard-exception-tabbed-add-dialog>
` : ''}
${this.showEditDialog_ ? html`
  <tab-discard-exception-edit-dialog
      @close="${this.onEditDialogClose_}" .ruleToEdit="${this.selectedRule_}">
  </tab-discard-exception-edit-dialog>
` : ''}<!--_html_template_end_-->`;
}
