// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import type {TableElement} from './table.js';
import {$$} from './utils.js';

/**
 * @fileoverview
 * DragAndDropManager facilitates drag-and-drop interactions within a
 * `product-specifications-table` HTMLElement, allowing users to reorder columns
 * by dragging them over one another.
 *
 * Note: DragAndDropManager relies on the table being a CSS Grid.
 *
 * Upon the table's owner document firing a 'dragstart' event,
 * DragAndDropManager adds listeners to the owner document and table for
 * relevant drag events.
 *
 * When the dragging column hovers over another column, its potential new
 * position is visually indicated by updating its 'order' CSS property.
 *
 * Releasing the dragging column over a valid drop target (another column),
 * notifies the `product-specifications-table` to update its internal column
 * order and, subsequently, their DOM positions. Listeners are then removed from
 * the document and table for non-'dragstart' drag events.
 *
 * Leaving the table mid-drag drops the dragging column at the position where
 * the 'dragleave' event was triggered.
 *
 * Once drag-and-drop ends regardless of its outcome (success, failure, or
 * cancellation), each column's CSS 'order' property is removed, to ensure the
 * visual layout matches the underlying data structure.
 */

export const IS_FIRST_COLUMN_ATTR: string = 'is-first-column';

function getColumnByComposedPath(path: EventTarget[]): HTMLElement|null {
  return path.filter(node => node instanceof HTMLElement)
             .find(p => (p).classList.contains('col')) ||
      null;
}

function getVisualOrderIndex(col: HTMLElement) {
  assert(col.style.order !== '');
  return Number.parseInt(col.style.order, 10);
}

export class DragAndDropManager {
  private dragImage_: HTMLImageElement;
  private eventTracker_: EventTracker = new EventTracker();
  private tableElement_: TableElement;
  private pendingOrder_: Map<number, number> = new Map();

  constructor(tableElement: TableElement) {
    // Create a transparent 1x1 pixel image that will replace the default drag
    // "ghost" image. The image is preloaded to ensure it's available when
    // dragging starts.
    this.dragImage_ = new Image(1, 1);
    this.dragImage_.src =
        'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAA' +
        'ABAAEAAAICTAEAOw==';
    this.tableElement_ = tableElement;
    this.eventTracker_.add(
        document, 'dragstart', (e: DragEvent) => this.onDragStart_(e));
  }

  private get columnElements_(): HTMLElement[] {
    const table = $$<HTMLElement>(this.tableElement_, '#table');
    assert(table);
    return Array.from(
        table.querySelectorAll<HTMLElement>('.col:not([hidden])'));
  }

  destroy(): void {
    this.eventTracker_.removeAll();
  }

  tablePropertiesUpdated(): void {
    // The table is re-rendering and will need the "style.order" replaced as it
    // will have been removed during the render.
    if (this.pendingOrder_.size > 0) {
      this.pendingOrder_.forEach((value, key) => {
        const col = this.columnElements_[key];
        assert(col);
        col.style.order = `${value}`;
        // Ensure the 'relative' position property is maintained while dragging.
        col.style.position = 'relative';
        col.toggleAttribute(IS_FIRST_COLUMN_ATTR, value === 0);
      });
    }
  }

  private syncVisualOrderWithDom() {
    this.columnElements_.forEach((column, index) => {
      // `is-first-column` ensures the first column has necessary styling.
      column.toggleAttribute(IS_FIRST_COLUMN_ATTR, index === 0);
      column.style.order = `${index}`;
      this.pendingOrder_.set(index, index);
    });
  }

  private resetVisualOrder() {
    this.pendingOrder_.clear();
    for (const column of this.columnElements_) {
      column.style.order = '';
    }
  }

  private onDragStart_(e: DragEvent) {
    if (e.dataTransfer) {
      // Replace the ghost image that appears when dragging with a transparent
      // 1x1 pixel image.
      e.dataTransfer.setDragImage(this.dragImage_, 0, 0);
    }

    const dragElement = getColumnByComposedPath(e.composedPath());
    if (!dragElement) {
      return;
    }

    this.dragStart_(dragElement);

    this.eventTracker_.add(
        document, 'dragover', (e: DragEvent) => this.dragOver_(e));
    this.eventTracker_.add(document, 'drop', (e: DragEvent) => this.drop_(e));
    // Drops the dragging column if it leaves the table. This ensures
    // that 'dragover' events only fire for adjacent columns.
    this.eventTracker_.add(
        this.tableElement_, 'dragleave', (e: DragEvent) => this.drop_(e));
    this.eventTracker_.add(document, 'dragend', () => {
      this.eventTracker_.remove(document, 'dragover');
      this.eventTracker_.remove(document, 'drop');
      this.eventTracker_.remove(this.tableElement_, 'dragleave');
      this.dragEnd_();
    });
  }

  // Sets up column reordering for drag events.
  private dragStart_(dragElement: HTMLElement) {
    // Set the column element positions to relative so that dragging functions
    // correctly.
    this.columnElements_.forEach((column) => {
      column.style.position = 'relative';
    });

    this.tableElement_.draggingColumn = dragElement;
    // Set initial column order for later visual reordering.
    this.syncVisualOrderWithDom();
  }

  // Swaps the visual order of the dragging column with the adjacent column it
  // drags over.
  // Note: DOM node positions are unaffected.
  // TODO(b/354729553): Handle fast column drags.
  private dragOver_(e: DragEvent) {
    e.preventDefault();

    const dragElement = this.tableElement_.draggingColumn;
    if (!dragElement) {
      return;
    }

    const dropTarget = getColumnByComposedPath(e.composedPath());
    if (!dropTarget) {
      return;
    }

    const fromIndex = getVisualOrderIndex(dragElement);
    const toIndex = getVisualOrderIndex(dropTarget);
    if (toIndex !== fromIndex) {
      if (e.dataTransfer) {
        e.dataTransfer.dropEffect = 'move';
      }

      // The position of the element in the DOM will be different from its
      // visual position when rendered. The DOM position is the one that needs
      // to be tracked per-render.
      const domFromIndex =
          this.columnElements_.findIndex(item => item === dragElement);
      const domToIndex =
          this.columnElements_.findIndex(item => item === dropTarget);
      this.pendingOrder_.set(domToIndex, fromIndex);
      this.pendingOrder_.set(domFromIndex, toIndex);

      dropTarget.style.order = `${fromIndex}`;
      dragElement.style.order = `${toIndex}`;

      // `is-first-column` ensures the first column has necessary styling.
      dropTarget.toggleAttribute(IS_FIRST_COLUMN_ATTR, fromIndex === 0);
      dragElement.toggleAttribute(IS_FIRST_COLUMN_ATTR, toIndex === 0);
    }
  }

  // Updates DOM node positions to reflect drag/drop result.
  private drop_(e: DragEvent) {
    const dragElement = this.tableElement_.draggingColumn;
    if (!dragElement) {
      return;
    }

    const dropTarget = getColumnByComposedPath(e.composedPath());
    if (!dropTarget) {
      return;
    }

    const fromIndex = Number(dragElement.dataset['index']);
    const toIndex = getVisualOrderIndex(dropTarget);
    if (toIndex !== fromIndex) {
      this.tableElement_.moveColumnOnDrop(fromIndex, toIndex);
      this.pendingOrder_.clear();
    }

    this.dragEnd_();
  }

  // Called when drag-and-drop is finished, even if canceled.
  private dragEnd_() {
    // Once dragging has finished, unset the position attribute to avoid
    // tooltips and other popovers from being clipped by the column.
    this.columnElements_.forEach((column) => {
      column.style.position = 'unset';
    });

    if (!this.tableElement_.draggingColumn) {
      return;
    }

    this.tableElement_.draggingColumn = null;
    // Remove each column's CSS 'order' property, to ensure the visual
    // layout matches the underlying data structure.
    this.resetVisualOrder();
  }
}
