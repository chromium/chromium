// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchEngineEntryElement} from './search_engine_entry.js';

export function getHtml(this: SettingsSearchEngineEntryElement) {
  return html`<!--_html_template_start_-->
<div class="list-item cr-row" role="row">
  <span role="cell" id="name-column">
    <settings-search-engine-icon .engine="${this.engine}">
    </settings-search-engine-icon>
    <div>${this.engine.displayName}</div>
  </span>
  <span class="additional-info-column-group">
    <span role="cell" id="shortcut-column" ?hidden="${!this.showShortcut}">
      <div>${this.engine.keyword}</div>
    </span>
    <span role="cell" id="url-column" ?hidden="${!this.showQueryUrl}">
      <div class="text-elide">${this.engine.url}</div>
    </span>
    <span role="cell" id="controls-column-group">
      ${!this.searchSettingsUpdateEnabled_ ? html`
        <cr-button class="secondary-button"
            @click="${this.onActivateClick_}"
            aria-label="${this.getActivateButtonAriaLabel_()}"
            ?hidden="${!this.engine.canBeActivated}" id="activateButton">
          $i18n{searchEnginesActivate}
        </cr-button>
      ` : ''}
      <cr-button class="secondary-button"
          @click="${this.onViewOrEditClick_}"
          ?hidden="${!this.shouldShowSecondaryButton_()}"
          id="viewDetailsButton">
        $i18n{searchEnginesViewDetails}
      </cr-button>
      ${!this.searchSettingsUpdateEnabled_ ? html`
        <cr-icon-button class="icon-edit"
            @click="${this.onViewOrEditClick_}"
            title="$i18n{edit}" ?hidden="${!this.shouldShowEditIcon_()}"
            aria-label="${this.getEditButtonAriaLabel_()}"
            .disabled="${!this.engine.canBeEdited}" id="editIconButton">
        </cr-icon-button>
      ` : ''}
      ${this.engine.isManaged ? html`
        <cr-policy-indicator indicator-type="userPolicy">
        </cr-policy-indicator>
      ` : ''}
      <cr-icon-button class="icon-more-vert" @click="${this.onDotsClick_}"
          ?disabled="${this.shouldDisableDots_()}"
          title="$i18n{moreActions}"
          aria-label="${this.getMoreActionsAriaLabel_()}">
      </cr-icon-button>
      <cr-action-menu role-description="$i18n{menu}">
        ${!this.searchSettingsUpdateEnabled_ ? html`
          <button class="dropdown-item" @click="${this.onMakeDefaultClick_}"
              ?disabled="${!this.engine.canBeDefault}" id="makeDefault">
            $i18n{searchEnginesMakeDefault}
          </button>
          <button class="dropdown-item" @click="${this.onDeactivateClick_}"
              ?hidden="${!this.engine.canBeDeactivated}" id="deactivate">
            $i18n{searchEnginesDeactivate}
          </button>
          <button class="dropdown-item" @click="${this.onDeleteClick_}"
              ?hidden="${!this.engine.canBeRemoved}" id="delete">
            $i18n{delete}
          </button>
        ` : ''}
        ${this.searchSettingsUpdateEnabled_ ? html`
          <button class="dropdown-item" @click="${this.onViewOrEditClick_}"
              ?hidden="${!this.showEditOption_()}" id="editOption">
            $i18n{edit}
          </button>
          <button class="dropdown-item" @click="${this.onActivateClick_}"
              ?hidden="${!this.engine.canBeActivated}" id="activateOption">
            ${this.turnOnLabel_()}
          </button>
          <button class="dropdown-item" @click="${this.onDeactivateClick_}"
              ?hidden="${!this.showDeactivateOption_()}"
              ?disabled="${!this.engine.canBeDeactivated}"
              id="deactivateOption">
            ${this.turnOffLabel_()}
          </button>
          <button class="dropdown-item" @click="${this.onDeleteClick_}"
              ?hidden="${!this.showDeleteOption_()}"
              ?disabled="${!this.engine.canBeRemoved}" id="deleteOption">
            $i18n{delete}
          </button>
          <button class="dropdown-item" @click="${this.onMakeDefaultClick_}"
              ?hidden="${!this.showMakeDefaultOption_()}"
              ?disabled="${!this.engine.canBeDefault}"
              id="makeDefaultOption">
            $i18n{searchEnginesMakeDefault}
          </button>
          <!-- Options only available for extensions -->
          <button class="dropdown-item" @click="${this.onDisableClick_}"
              ?hidden="${!this.showDisableExtensionOption_()}"
              id="disableExtensionOption">
            $i18n{searchDisableExtension}
          </button>
          <button class="dropdown-item" @click="${this.onManageClick_}"
              ?hidden="${!this.engine.isOmniboxExtension}"
              id="manageExtensionOption">
            $i18n{searchManageExtension}
            <div id="externalIcon" class="cr-icon icon-external"></div>
          </button>
        ` : ''}
      </cr-action-menu>
    </span>
  </span>
</div>
${this.showControlledIndicator_() ? html`
  <extension-controlled-indicator
      extension-id="${this.engine.extension!.id}"
      extension-name="${this.engine.extension!.name}"
      extension-can-be-disabled="${this.engine.extension!.canBeDisabled}">
  </extension-controlled-indicator>
` : ''}
<!--_html_template_end_-->`;
}
