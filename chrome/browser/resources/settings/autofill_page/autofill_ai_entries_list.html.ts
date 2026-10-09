// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAutofillAiEntriesListElement} from './autofill_ai_entries_list.js';

export function getHtml(this: SettingsAutofillAiEntriesListElement) {
  return html`<!--_html_template_start_-->
<div id="entriesHeader" class="cr-row">
  <h2 class="flex">
    ${this.listTitle}
  </h2>
  <!-- Disabled: If the user is not eligible for Autofill with Ai or if the
       user opted out by switching the toggle, the user should not be able to
       add entity instances. -->
  <cr-button id="addEntityInstance" class="header-aligned-button"
      ?hidden="${!this.completeEntityTypesList_.length}"
      ?disabled="${!this.allowNewEntitiesAddition_}"
      @click="${this.onAddEntityInstanceClick_}">
    $i18n{add}
    <cr-icon icon="cr:arrow-drop-down" class="arrow-icon-down"></cr-icon>
  </cr-button>
</div>
<ul id="entries" class="list-frame vertical-list">
  ${this.entityInstances_.map((item, index) => html`
    <li class="list-item">
      <div class="entity-type-icon"
          ?hidden="${!this.typeNameToIconName_(item.type.typeName)}">
        <cr-icon icon="${this.typeNameToIconName_(item.type.typeName)}"></cr-icon>
      </div>
      <div class="start">
        <div class="ellipses">${item.entityInstanceLabel}</div>
        <div class="ellipses cr-secondary-text">
          ${item.entityInstanceSubLabel}
        </div>
      </div>
      <div id="walletIndicator" ?hidden="${!item.storedInWallet}">
<if expr="_google_chrome">
        <cr-icon aria-hidden="true"
            icon="settings-internal:google-wallet">
        </cr-icon>
</if>
<if expr="not _google_chrome">
        <span class="sub-label">$i18n{googleWallet}</span>
</if>
      </div>
      ${item.storedInWallet ? html`
        <cr-icon-button class="icon-external" id="remoteWalletPassesLink"
            title="$i18n{remoteWalletPassesLinkLabel}" role="link"
            data-index="${index}"
            @click="${this.onRemoteWalletPassesLinkClick_}"
            aria-description="$i18n{opensInNewTab}">
        </cr-icon-button>
      ` : html`
        <cr-icon-button id="moreButton" class="icon-more-vert"
            data-index="${index}"
            @click="${this.onMoreButtonClick_}"
            title="${this.i18n('autofillAiMoreActionsForEntityInstance',
                item.entityInstanceLabel, item.entityInstanceSubLabel)}">
        </cr-icon-button>
      `}
    </li>
  `)}
  <li id="entriesNone" class="list-item" ?hidden="${this.entityInstances_.length > 0}">
    $i18n{autofillAiEntityInstancesNone}
  </li>
</ul>

<cr-lazy-render-lit id="addMenu"
    .template="${() => html`
      <cr-action-menu role-description="$i18n{menu}">
        ${this.completeEntityTypesList_.map((item, index) => html`
          <button id="addSpecificEntityType" class="dropdown-item"
              data-index="${index}"
              @click="${this.onAddEntityInstanceFromDropdownClick_}">
            <div>${item.typeNameAsString}</div>
          </button>
        `)}
      </cr-action-menu>
    `}">
</cr-lazy-render-lit>

<cr-lazy-render-lit id="actionMenu"
    .template="${() => html`
      <cr-action-menu role-description="$i18n{menu}">
        <button id="menuEditEntityInstance" class="dropdown-item"
            @click="${this.onMenuEditEntityInstanceClick_}">$i18n{edit}</button>
        <button id="menuRemoveEntityInstance" class="dropdown-item"
            @click="${this.onMenuRemoveEntityInstanceClick_}">$i18n{delete}</button>
      </cr-action-menu>
    `}">
</cr-lazy-render-lit>

${this.showAddOrEditEntityInstanceDialog_ ? html`
  <settings-autofill-ai-add-or-edit-dialog id="addOrEditEntityInstanceDialog"
      .entityInstance="${this.activeEntityInstance_}"
      .dialogTitle="${this.addOrEditEntityInstanceDialogTitle_}"
      @autofill-ai-add-or-edit-done="${this.onAutofillAiAddOrEditDone_}"
      @close="${this.onAddOrEditEntityInstanceDialogClose_}">
  </settings-autofill-ai-add-or-edit-dialog>
` : ''}
${this.showRemoveEntityInstanceDialog_ ? html`
  <settings-simple-confirmation-dialog id="removeEntityInstanceDialog"
      title-text="${this.activeEntityInstanceDeleteTitle_}"
      body-text="$i18n{autofillAiDeleteEntityInstanceDialogText}"
      confirm-text="$i18n{delete}"
      @close="${this.onRemoveEntityInstanceDialogClose_}">
  </settings-simple-confirmation-dialog>
` : ''}
<!--_html_template_end_-->`;
}
