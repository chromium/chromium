// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryItemElement} from './history_item.js';

export function getHtml(this: HistoryItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="main-container">
      <div id="background-clip" aria-hidden="true">
        <div id="background"></div>
      </div>
      <div id="date-accessed" class="card-title" role="row">
        <div role="rowheader">
          <div role="heading" aria-level="2">
            ${this.cardTitle_()}
          </div>
        </div>
      </div>
      <div role="row" @mousedown="${this.onRowMousedown_}" @click="${this.onRowClick_}">
        <div id="item-container" focus-row-container>
          <div role="gridcell">
            <cr-checkbox id="checkbox" .checked="${this.selected}"
                focus-row-control focus-type="cr-checkbox"
                @mousedown="${this.onCheckboxClick_}" @keydown="${this.onCheckboxClick_}"
                @change="${this.onCheckboxChange_}" class="no-label"
                ?hidden="${this.selectionNotAllowed_}"
                .disabled="${this.selectionNotAllowed_}">
              ${this.getEntrySummary_()}
            </cr-checkbox>
          </div>
          <!-- ARIA hidden to avoid redundancy since timestamp is already part of
              |getEntrySummary_|. -->
          <span id="time-accessed" aria-hidden="true">
            ${this.item?.readableTimestamp}
          </span>
          <div role="gridcell" id="item-info">
            <div id="title-and-domain">
              <a href="${this.item?.url}" id="link" class="website-link"
                  focus-row-control focus-type="link"
                  title="${this.item?.title}" @click="${this.onLinkClick_}"
                  @auxclick="${this.onLinkClick_}" @contextmenu="${this.onLinkRightClick_}"
                  aria-describedby="${this.getAriaDescribedByForHeading_()}">
                <div class="website-icon" id="icon"></div>
                <history-searched-label class="website-title"
                    title="${this.item?.title}"
                    search-term="${this.searchTerm}"></history-searched-label>
              </a>
              <span id="domain">${this.item?.domain}</span>
            </div>
            <div id="icons">
              ${this.shouldShowActorTooltip_() ? html`
                <cr-tooltip-icon id="actor-icon" icon-class="history20:auto-nav"
                    tooltip-text="$i18n{actorTaskTooltip}"
                    icon-aria-label="$i18n{actorTaskTooltip}">
                </cr-tooltip-icon>
              `: ''}
              ${this.item?.starred ? html`
                <cr-icon-button id="bookmark-star" iron-icon="cr:star"
                    @click="${this.onRemoveBookmarkClick_}"
                    title="$i18n{removeBookmark}"
                    aria-hidden="true">
                </cr-icon-button>
                `: ''}
            </div>
          </div>
          <div role="gridcell" id="options">
            <cr-icon-button id="menu-button" iron-icon="cr:more-vert"
                focus-row-control focus-type="cr-menu-button"
                title="$i18n{actionMenuDescription}" @click="${this.onMenuButtonClick_}"
                @keydown="${this.onMenuButtonKeydown_}"
                aria-haspopup="menu"
                aria-describedby="${this.getAriaDescribedByForActions_()}">
            </cr-icon-button>
          </div>
        </div>
        ${this.item?.debug ? html`
          <div id="debug-container" aria-hidden="true">
            <div class="debug-info">DEBUG</div>
            <div class="debug-info"
                ?hidden="${!this.item?.debug.isUrlInLocalDatabase}">
              in local data
            </div>
            <div class="debug-info" ?hidden="${!this.item?.isUrlInRemoteUserData}">
              in remote data
            </div>
            <div class="debug-info"
                ?hidden="${!this.item?.debug.isUrlInLocalDatabase}">
              typed count: ${this.item?.debug.typedCount}
            </div>
            <div class="debug-info"
                hidden="${!this.item?.debug.isUrlInLocalDatabase}">
              visit count: ${this.item?.debug.visitCount}
            </div>
          </div>`: ''}
        <div id="time-gap-separator" ?hidden="${!this.hasTimeGap}"></div>
      </div>
    </div>
<!--_html_template_end_-->`;
  // clang-format on
}
