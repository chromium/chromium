// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles processing and displaying braille in the ChromeVox
 * panel.
 */

export class PanelCaptions {
  constructor() {
    /** @private {!BrailleCaptions} */
    this.braille_ = new BrailleCaptions();
  }

  static init() {
    if (PanelCaptions.instance) {
      throw new Error('Cannot create two PanelCaptions instances');
    }
    PanelCaptions.instance = new PanelCaptions();
  }

  static get braille() {
    return PanelCaptions.instance.braille_;
  }
}

class BrailleCaptions {
  /** @param {!Element} cell */
  static addBorders(cell) {
    if (cell.tagName === 'TD') {
      cell.className = 'highlighted-cell';
      const companionIDs = cell.getAttribute('data-companionIDs');
      companionIDs.split(' ').forEach(
          companionID => $(companionID).className = 'highlighted-cell');
    }
  }

  /** @param {!Element} cell */
  static removeBorders(cell) {
    if (cell.tagName === 'TD') {
      cell.className = 'unhighlighted-cell';
      const companionIDs = cell.getAttribute('data-companionIDs');
      companionIDs.split(' ').forEach(
          companionID => $(companionID).className = 'unhighlighted-cell');
    }
  }

  /** @param {!Element} cell */
  static routeCursor(cell) {
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
}

/** @type {!PanelCaptions} */
PanelCaptions.instance;

const $ = id => document.getElementById(id);
