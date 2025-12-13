// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './icon_container.css.js';

const MAX_CELL_COUNT = 4;

export class IconContainerElement extends CrLitElement {
  static get is() {
    return 'ntp-icon-container';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    const overflowCount = this.totalTabCount - MAX_CELL_COUNT + 1;
    const overflowCells = Array(overflowCount > 1 ? 1 : 0).fill(overflowCount);
    const faviconCells =
        this.faviconUrls.slice(0, MAX_CELL_COUNT - overflowCells.length);
    const emptyCells =
        Array(MAX_CELL_COUNT - faviconCells.length - overflowCells.length)
            .fill(null);

    // clang-format off
    return html`
      <div class="icons-container">
        ${faviconCells.map(url => this.renderIconCell(url))}
        ${emptyCells.map(() => this.renderEmptyCell())}
        ${overflowCells.map(() => this.renderOverflowCell(overflowCount))}
      </div>
    `;
    // clang-format on
  }

  static override get properties() {
    return {
      faviconUrls: {type: Array},
      totalTabCount: {type: Number},
    };
  }

  accessor faviconUrls: string[] = [];
  accessor totalTabCount: number = 0;

  private renderEmptyCell() {
    return html`<div class="cell empty"></div>`;
  }

  private renderIconCell(url: string) {
    // clang-format off
    return html`
      <div class="cell icon">
        <div class="icon"
            style="background-image: ${getFaviconForPageURL(url, false)};">
        </div>
      </div>
    `;
    // clang-format on
  }

  private renderOverflowCell(count: number) {
    // clang-format off
    return html`
      <div class="cell overflow-count" aria-hidden="true">
        ${count <= 99 ? html`+${count}` : '99+'}
      </div>
    `;
    // clang-format on
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-icon-container': IconContainerElement;
  }
}

customElements.define(IconContainerElement.is, IconContainerElement);
