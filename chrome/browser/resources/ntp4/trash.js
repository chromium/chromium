// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DragWrapper, DragWrapperDelegate} from 'chrome://resources/js/drag_wrapper.js';
import {getCurrentlyDraggingTile, setCurrentDropEffect} from './tile_page.js';

/**
 * @fileoverview Trash
 * This is the class for the trash can that appears when dragging an app.
 */


/**
 * @constructor
 * @extends {HTMLDivElement}
 * @implements {DragWrapperDelegate}
 */
export function Trash(trash) {
  trash.__proto__ = Trash.prototype;
  trash.initialize();
  return trash;
}

Trash.prototype = {
  __proto__: HTMLDivElement.prototype,

  initialize(element) {
    this.dragWrapper_ = new DragWrapper(this, this);
  },

  /**
   * Determines whether we are interested in the drag data for |e|.
   * @param {Event} e The event from drag enter.
   * @return {boolean} True if we are interested in the drag data for |e|.
   */
  shouldAcceptDrag(e) {
    const tile = getCurrentlyDraggingTile();
    if (!tile) {
      return false;
    }

    return tile.firstChild.canBeRemoved();
  },

  /** @override */
  doDragOver(e) {
    getCurrentlyDraggingTile().dragClone.classList.add('hovering-on-trash');
    setCurrentDropEffect(e.dataTransfer, 'move');
    e.preventDefault();
  },

  /** @override */
  doDragEnter(e) {
    this.doDragOver(e);
  },

  /** @override */
  doDrop(e) {
    e.preventDefault();

    const tile = getCurrentlyDraggingTile();
    tile.firstChild.removeFromChrome();
    tile.landedOnTrash = true;
  },

  /** @override */
  doDragLeave(e) {
    getCurrentlyDraggingTile().dragClone.classList.remove('hovering-on-trash');
  },
};
