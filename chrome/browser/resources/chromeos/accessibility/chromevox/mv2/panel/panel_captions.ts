// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles processing and displaying braille in the ChromeVox
 * panel.
 */

class BrailleCaptions {
  private brailleTableElement_ = $('braille-table') as HTMLTableElement;
  private brailleTableElement2_ = $('braille-table2') as HTMLTableElement;

  addBorders(cell: HTMLTableCellElement): void {
    if (cell.tagName === 'TD') {
      cell.className = 'highlighted-cell';
      const companionIDs = cell.getAttribute('data-companionIDs');
      companionIDs!.split(' ').forEach(
          companionID => $(companionID)!.className = 'highlighted-cell');
    }
  }

  clearTables(): void {
    this.clearTable_(this.brailleTableElement_);
    this.clearTable_(this.brailleTableElement2_);
  }

  removeBorders(cell: HTMLTableCellElement): void {
    if (cell.tagName === 'TD') {
      cell.className = 'unhighlighted-cell';
      const companionIDs = cell.getAttribute('data-companionIDs');
      companionIDs!.split(' ').forEach(
          companionID => $(companionID)!.className = 'unhighlighted-cell');
    }
  }

  routeCursor(cell: HTMLTableCellElement): void {
    if (cell.tagName === 'TD') {
      const displayPosition = parseInt(cell.id.split('-')[0], 10);
      if (Number.isNaN(displayPosition)) {
        throw new Error(
            'The display position is calculated assuming that the cell ID ' +
            'is formatted like int-string. For example, 0-brailleCell is a ' +
            'valid cell ID.');
      }
      chrome.extension.getBackgroundPage()['ChromeVox'].braille.route(
          displayPosition);
    }
  }

  private clearTable_(table: HTMLTableElement): void {
    const rowCount = table.rows.length;
    for (let i = 0; i < rowCount; i++) {
      table.deleteRow(0);
    }
  }
}

function $(id: string): HTMLElement | null {
  return document.getElementById(id);
}

export namespace PanelCaptions {
  export let braille: BrailleCaptions;

  export function init(): void {
    if (braille) {
      throw new Error('Cannot create two PanelCaptions instances');
    }
    braille = new BrailleCaptions();
  }
}