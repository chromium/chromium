// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsRuntimeHostPermissionsElement} from './runtime_host_permissions.js';

export function getHtml(this: ExtensionsRuntimeHostPermissionsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${!this.enableEnhancedSiteControls ? html`
  <div id="permissions-mode">
    <div id="section-heading">
      <div id="section-heading-heading">
        <span id="section-heading-text">
          $i18n{hostPermissionsHeading}
        </span>
        <a class="link-icon-button"
            aria-label="$i18n{permissionsLearnMoreLabel}"
            href="$i18n{hostPermissionsLearnMoreLink}" target="_blank"
            @click="${this.onLearnMoreClick_}">
          <cr-icon icon="cr:help-outline"></cr-icon>
        </a>
      </div>
      <div>
        <select id="hostAccess" class="md-select"
            @change="${this.onHostAccessChange_}"
            aria-labelledby="section-heading-text">
          <option value="${chrome.developerPrivate.HostAccess.ON_CLICK}"
              ?selected="${this.isHostAccessSelected_(
                  chrome.developerPrivate.HostAccess.ON_CLICK)}">
            $i18n{hostAccessOnClick}
          </option>
          <option
              value="${chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES}"
              ?selected="${this.isHostAccessSelected_(
                  chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES)}">
            $i18n{hostAccessOnSpecificSites}
          </option>
          <option value="${chrome.developerPrivate.HostAccess.ON_ALL_SITES}"
              ?selected="${this.isHostAccessSelected_(
                  chrome.developerPrivate.HostAccess.ON_ALL_SITES)}">
            $i18n{hostAccessOnAllSites}
          </option>
        </select>
      </div>
    </div>
  </div>` : html`
  <div id="new-permissions-mode">
    <div id="new-section-heading">
      <div id="new-section-heading-title">
        <span id="new-section-heading-text">
            $i18n{newHostPermissionsHeading}
        </span>
        <a class="link-icon-button"
            aria-label="$i18n{permissionsLearnMoreLabel}"
            href="$i18n{hostPermissionsLearnMoreLink}" target="_blank"
            @click="${this.onLearnMoreClick_}">
          <cr-icon icon="cr:help-outline"></cr-icon>
        </a>
      </div>
      <span id="new-section-heading-subtext">
        $i18n{hostPermissionsSubHeading}
      </span>
      <div id="host-access-row">
        <select id="newHostAccess" class="md-select"
            @change="${this.onHostAccessChange_}"
            aria-labelledby="new-section-heading-text">
          <option value="${chrome.developerPrivate.HostAccess.ON_CLICK}"
              ?selected="${this.isHostAccessSelected_(
                  chrome.developerPrivate.HostAccess.ON_CLICK)}">
            $i18n{hostAccessAskOnEveryVisit}
          </option>
          <option
              value="${chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES}"
              ?selected="${this.isHostAccessSelected_(
                  chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES)}">
            $i18n{hostAccessAllowOnSpecificSites}
          </option>
          <option value="${chrome.developerPrivate.HostAccess.ON_ALL_SITES}"
              ?selected="${this.isHostAccessSelected_(
                  chrome.developerPrivate.HostAccess.ON_ALL_SITES)}">
            $i18n{hostAccessAllowOnAllSites}
          </option>
        </select>
        <cr-button id="add-site-button" ?hidden="${!this.showSpecificSites_()}"
            @click="${this.onAddHostClick_}">
          $i18n{add}
        </cr-button>
      </div>
    </div>
  </div>`}

${this.showSpecificSites_() ? html`
  <ul id="hosts">
    ${this.getRuntimeHosts_().map((item, index) => html`
      <li>
        <div class="site-favicon"
            .style="background-image:${this.getFaviconUrl_(item)}"
            ?hidden="${!this.enableEnhancedSiteControls}">
        </div>
        <div class="site">${item}</div>
        <cr-icon-button class="icon-edit edit-host"
            @click="${this.onEditHostClick_}" data-index="${index}"
            ?hidden="${!this.enableEnhancedSiteControls}">
        </cr-icon-button>
        <cr-icon-button class="icon-delete-gray remove-host"
            @click="${this.onDeleteHostClick_}" data-index="${index}"
            ?hidden="${!this.enableEnhancedSiteControls}">
        </cr-icon-button>
        <cr-icon-button class="icon-more-vert open-edit-host"
            @click="${this.onOpenEditHostClick_}" data-index="${index}"
            title="$i18n{hostPermissionsEdit}"
            ?hidden="${this.enableEnhancedSiteControls}">
        </cr-icon-button>
      </li>`)}
    <li ?hidden="${this.enableEnhancedSiteControls}">
      <a id="add-host" is="action-link" @click="${this.onAddHostClick_}">
        $i18n{itemSiteAccessAddHost}
      </a>
    </li>
  </ul>` : ''}

<cr-action-menu id="hostActionMenu" role-description="$i18n{menu}">
  <button class="dropdown-item" id="action-menu-edit"
      @click="${this.onActionMenuEditClick_}">
    $i18n{hostPermissionsEdit}
  </button>
  <button class="dropdown-item" id="action-menu-remove"
      @click="${this.onActionMenuRemoveClick_}">
    $i18n{remove}
  </button>
</cr-action-menu>
${this.showHostDialog_ ? html`
  <extensions-runtime-hosts-dialog .delegate="${this.delegate}"
      .itemId="${this.itemId}"
      .enableEnhancedSiteControls="${this.enableEnhancedSiteControls}"
      .currentSite="${this.hostDialogModel_}"
      .updateHostAccess="${this.dialogShouldUpdateHostAccess_()}"
      @close="${this.onHostDialogClose_}" @cancel="${this.onHostDialogCancel_}">
  </extensions-runtime-hosts-dialog>` : ''}
${this.showRemoveSiteDialog_ ? html`
  <cr-dialog id="removeSitesDialog"
      @cancel="${this.onRemoveSitesWarningCancel_}" show-on-attach>
    <div slot="title">$i18n{removeSitesDialogTitle}</div>
    <div slot="button-container">
      <cr-button class="cancel-button"
          @click="${this.onRemoveSitesWarningCancel_}">
        $i18n{cancel}
      </cr-button>
      <cr-button class="action-button"
          @click="${this.onRemoveSitesWarningConfirm_}">
        $i18n{remove}
      </cr-button>
    </div>
  </cr-dialog>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
