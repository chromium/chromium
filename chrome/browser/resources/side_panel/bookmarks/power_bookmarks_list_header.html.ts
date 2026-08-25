// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarksListHeaderElement} from './power_bookmarks_list_header.js';

export function getHtml(this: PowerBookmarksListHeaderElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="header-container">
  <div id="navigation-container" class="${this.getTitleContainerClass_()}">
    <div id="root-header" class="${this.getRootHeaderClass_()}">
      <h1 class="title-span active">$i18n{allBookmarks}</h1>
    </div>

    <sp-heading id="folder-header" class="${this.getFolderHeaderClass_()}"
        back-button-title="$i18n{tooltipBack}"
        back-button-aria-label="${this.getBackButtonLabel_()}"
        @back-button-click="${this.onBackButtonClick_}"
        ?hide-back-button="${this.shouldHideBackButton_()}"
        ?disable-back-button="${this.disableBackButton_()}">
      <h1 slot="heading" id="title-container"
          class="${this.getTitleContainerClass_()}">
        <span id="title-a"
            class="${this.getTitleClass_('a')}">${this.titleA_}</span>
        <span id="title-b"
            class="${this.getTitleClass_('b')}">${this.titleB_}</span>
      </h1>
    </sp-heading>
  </div>

  <div id="actions-container">
    <div aria-hidden="true" class="metadata">
      ${this.activeSortType_.label}
    </div>
    <cr-icon-button class="sort-menu-button"
        iron-icon="${this.webuiRoundedIconsEnabled_
            ? 'sp:filter-list'
            : 'sp:filter-list-old'}"
        title="$i18n{tooltipOrganize}"
        aria-label="$i18n{sortMenuA11yLabel}"
        aria-description="${this.activeSortType_.label}"
        @click="${this.onShowSortMenuClick_}">
    </cr-icon-button>
    <cr-icon-button id="viewButton"
        iron-icon="${this.getViewButtonIcon_()}"
        title="${this.getViewButtonTooltip_()}"
        aria-label="${this.getViewButtonA11yLabel_()}"
        @click="${this.onViewToggleClick_}">
    </cr-icon-button>
    <cr-icon-button id="editButton" class="icon-edit"
        ?disabled="${this.disableEdit}"
        title="$i18n{tooltipEdit}"
        aria-label="$i18n{editBookmarkListA11yLabel}"
        ?aria-pressed="${this.editing}"
        @click="${this.onBulkEditClick_}">
    </cr-icon-button>
  </div>
</div>

<cr-action-menu id="sortMenu" auto-close-on-focusout>
  ${this.sortTypes_.map((item, index) => html`
    <button class="dropdown-item" @click="${this.onSortTypeClick_}"
        data-index="${index}">
      <cr-icon icon="cr:check"
          ?invisible="${!this.sortMenuItemIsSelected_(item)}">
      </cr-icon>
      ${this.getSortMenuItemLowerLabel_(item)}
    </button>
  `)}
</cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
