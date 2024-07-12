// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {DomRepeat} from 'chrome://resources/polymer/v3_0/polymer/lib/elements/dom-repeat.js';

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
 * If a drop fails or is cancelled, column positions are restored to their
 * original state in the DOM to ensure the visual layout matches the underlying
 * data structure.
 */

function getColumnByComposedPath(path?: EventTarget[]): HTMLElement|null {
  if (!path) {
    return null;
  }

  for (let i = 0; i < path!.length; i++) {
    const element = path![i] as Element;
    if (element.className === 'col') {
      return path![i] as HTMLElement;
    }
  }
  return null;
}

function getVisualOrderIndex(col: HTMLElement) {
  assert(col.style.order !== '');
  return parseInt(col.style.order);
}

function syncVisualOrderWithDOM(columnElements: HTMLElement[]) {
  columnElements.forEach((column, index) => {
    // `is-first-column` ensures the first column has necessary styling.
    column.toggleAttribute('is-first-column', index === 0);
    column.style.order = `${index}`;
  });
}

export class DragAndDropManager {
  private eventTracker_: EventTracker = new EventTracker();
  private tableElement_: TableElement;

  private get columnElements_(): HTMLElement[] {
    const table = $$<HTMLElement>(this.tableElement_, '#table');
    assert(table);
    return Array.from(
        table.querySelectorAll<HTMLElement>('.col:not([hidden])'));
  }

  init(tableElement: TableElement): void {
    this.tableElement_ = tableElement;
    this.eventTracker_.add(
        document, 'dragstart', (e: DragEvent) => this.onDragStart_(e));
  }

  destroy(): void {
    this.eventTracker_.removeAll();
  }

  private onDragStart_(e: DragEvent) {
    if (e.dataTransfer) {
      // Remove the ghost image that appears when dragging.
      e.dataTransfer.setDragImage(new Image(), 0, 0);
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
    this.tableElement_.draggingColumn = dragElement;
    // Set initial column order for later visual reordering.
    syncVisualOrderWithDOM(this.columnElements_);
  }

  // Swaps the visual order of the dragging column with the adjacent column it
  // drags over.
  // Note: DOM node positions are unaffected.
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
      dropTarget.style.order = `${fromIndex}`;
      dragElement.style.order = `${toIndex}`;
      // `is-first-column` ensures the first column has necessary styling.
      dropTarget.toggleAttribute('is-first-column', fromIndex === 0);
      dragElement.toggleAttribute('is-first-column', toIndex === 0);
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

    const columnRepeat = $$<DomRepeat>(this.tableElement_, '#columnRepeat');
    assert(columnRepeat);
    const fromIndex = (columnRepeat.modelForElement(dragElement) as unknown as {
                        columnIndex: number,
                      }).columnIndex;
    const toIndex = getVisualOrderIndex(dropTarget);
    if (toIndex !== fromIndex) {
      this.tableElement_.moveColumnOnDrop(fromIndex, toIndex);
    }

    // Since drag-and-drop only works for adjacent columns, a drop is considered
    // successful regardless of whether `toIndex` and `fromIndex` don't match.
    // If they do match, the 'order' CSS property of each column should already
    // match its DOM position.
    this.tableElement_.draggingColumn = null;
  }

  // Called when drag-and-drop is finished, even if canceled.
  // If cancelled, the 'order' CSS property of each column is reset to match its
  // DOM position, to ensure the visual layout matches the underlying data
  // structure.
  private dragEnd_() {
    if (!this.tableElement_.draggingColumn) {
      return;
    }

    this.tableElement_.draggingColumn = null;
    // Ensure each column's CSS 'order' property aligns with its DOM position.
    syncVisualOrderWithDOM(this.columnElements_);
  }
}
