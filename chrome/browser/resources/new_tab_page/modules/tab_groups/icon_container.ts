// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './icon_container.css.js';
import {getHtml} from './icon_container.html.js';

const MAX_CELL_COUNT = 4;

export class IconContainerElement extends CrLitElement {
  static get is() {
    return 'ntp-icon-container';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      faviconUrls: {type: Array},
      totalTabCount: {type: Number},
    };
  }

  accessor faviconUrls: string[] = [];
  accessor totalTabCount: number = 0;

  protected getOverflowCount(): number {
    return this.totalTabCount - MAX_CELL_COUNT + 1;
  }

  protected getFaviconCells(): string[] {
    const overflowCount = this.getOverflowCount();
    const overflowCellsCount = overflowCount > 1 ? 1 : 0;
    return this.faviconUrls.slice(0, MAX_CELL_COUNT - overflowCellsCount);
  }

  protected getEmptyCells(): null[] {
    const overflowCount = this.getOverflowCount();
    const overflowCellsCount = overflowCount > 1 ? 1 : 0;
    const faviconCells = this.getFaviconCells();
    return Array(MAX_CELL_COUNT - faviconCells.length - overflowCellsCount)
        .fill(null);
  }

  protected shouldShowOverflow(): boolean {
    return this.getOverflowCount() > 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-icon-container': IconContainerElement;
  }
}

customElements.define(IconContainerElement.is, IconContainerElement);
