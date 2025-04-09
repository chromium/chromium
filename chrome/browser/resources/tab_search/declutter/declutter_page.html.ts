// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabData} from '../tab_data.js';

import type {DeclutterPageElement} from './declutter_page.js';

enum DeclutterType {
  STALE_TABS = 0,
  DUPLICATE_TABS = 1,
}

export function getHtml(this: DeclutterPageElement) {
  return html`<!--_html_template_start_-->
  <div id="declutterPage">
    <div id="header">
      ${
      this.showBackButton ? html`
        <cr-icon-button
            aria-label="${this.getBackButtonAriaLabel_()}"
            iron-icon="cr:arrow-back"
            @click="${this.onBackClick_}">
        </cr-icon-button>
      ` :
                            ''}
      ${
      this.dedupeEnabled ? html`
        <div class="title">$i18n{declutterTitle}</div>
      ` :
                           html`
        <div class="header-text">
          <div class="title">$i18n{declutterInactiveTitleNoDedupe}</div>
            ${this.staleTabDatas_.length === 0 ? '' : html`
              <div class="subheading">$i18n{declutterInactiveBody}</div>
            `}
        </div>
      `}
    </div>
    ${
      (this.staleTabDatas_.length === 0 &&
       this.duplicateTabDatas_.length === 0) ?
          html`
      <div class="empty-content">
        <div class="empty-title">$i18n{declutterEmptyTitle}</div>
        <div class="empty-subheading">${
              this.dedupeEnabled ?
                  html`$i18n{declutterEmptyBody}` :
                  html`$i18n{declutterEmptyBodyNoDedupe}`}</div>
      </div>
    ` :
          html`
      <div id="scrollable">
        ${
              this.staleTabDatas_.length > 0 ?
                  html`
          <div class="card">
            ${
                      this.dedupeEnabled ? html`
              <div class="header-text">
                <div class="title">$i18n{declutterInactiveTitle}</div>
                <div class="subheading">$i18n{declutterInactiveBody}</div>
              </div>
            ` :
                                           ''}
            <div id="staleTabList" class="tab-list">
              ${
                      this.staleTabDatas_.map(
                          (item) => getTabSearchItem.bind(this)(
                              item, DeclutterType.STALE_TABS))}
            </div>
          </div>
        ` :
                  ''}
        ${
              this.dedupeEnabled && this.duplicateTabDatas_.length > 0 ?
                  html`
          <div class="card">
            <div class="header-text">
              <div class="title">$i18n{declutterDuplicateTitle}</div>
              <div class="subheading">$i18n{declutterDuplicateBody}</div>
            </div>
            <div id="duplicateTabList" class="tab-list">
              ${
                      this.duplicateTabDatas_.map(
                          (item) => getTabSearchItem.bind(this)(
                              item, DeclutterType.DUPLICATE_TABS))}
            </div>
          </div>
        ` :
                  ''}
      </div>
      <cr-button class="action-button" @click="${this.onCloseTabsClick_}">
        $i18n{closeTabs}
      </cr-button>
    `}
  </div><!--_html_template_end_-->`;
}

function getTabSearchItem(
    this: DeclutterPageElement, data: TabData, declutterType: DeclutterType) {
  return html`
    <tab-search-item class="mwb-list-item" .data="${data}"
        close-button-icon="tab-search:remove"
        close-button-aria-label=
                      "${this.getCloseButtonAriaLabel_(data)}"
        close-button-tooltip="${this.getCloseButtonTooltip_()}"
        role="option"
        @keydown="${this.onTabKeyDown_}"
        @close="${
      declutterType === DeclutterType.STALE_TABS ? this.onStaleTabExclude_ :
                                                   this.onDuplicateTabExclude_}"
        @focus="${this.onTabFocus_}"
        @blur="${this.onTabBlur_}"
        size=${this.dedupeEnabled ? 'compact' : 'medium'}
        hide-url>
    </tab-search-item>
  `;
}
