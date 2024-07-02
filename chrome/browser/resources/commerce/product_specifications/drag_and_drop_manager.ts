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
 * When the dragged column hovers over another column, its potential new
 * position is visually indicated by updating its 'order' CSS property.
 *
 * Releasing the dragged column over a valid drop target (another column),
 * notifies `product-specifications-table` to update its internal column order
 * and, subsequently, their DOM positions.
 *
 * Dragging the element outside of the table cancels drag-and-drop.
 */

interface Position {
  drop: number;
  drag: number;
}

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

function shouldSwapIndexes(
    prevIndexes: Position|null, fromIndex: number, toIndex: number) {
  return !(prevIndexes && prevIndexes.drop === fromIndex &&
           prevIndexes.drag === toIndex) &&
      toIndex !== fromIndex;
}

export class DragAndDropManager {
  private eventTracker_: EventTracker = new EventTracker();
  private tableElement_: TableElement;
  private prevIndexes_: Position|null = null;

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
    this.eventTracker_.add(document, 'dragend', () => this.dragEnd_());
    // Ends drag-and-drop if the dragging column leaves the table. This ensures
    // that 'dragover' events only fire for adjacent columns.
    this.eventTracker_.add(
        this.tableElement_, 'dragleave', () => this.dragEnd_());
  }

  // Sets up column reordering for drag events.
  private dragStart_(dragElement: HTMLElement) {
    this.tableElement_.draggingColumn = dragElement;
    this.prevIndexes_ = null;
    const columnElements = this.columnElements_;
    // Set initial column order for later visual reordering.
    columnElements.forEach((column, index) => {
      column.style.order = `${index}`;
    });
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
    if (shouldSwapIndexes(this.prevIndexes_, fromIndex, toIndex)) {
      if (e.dataTransfer) {
        e.dataTransfer.dropEffect = 'move';
      }
      dropTarget.style.order = `${fromIndex}`;
      dragElement.style.order = `${toIndex}`;
      this.prevIndexes_ = {drag: fromIndex, drop: toIndex};
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

    this.dragEnd_();
  }

  // Called when drag-and-drop is finished (even if the drop was canceled).
  private dragEnd_() {
    // TODO(b/331955377): Update |this.tableElement_.columns| if a 'dragend'
    // event is fired before a 'drop' event is.
    this.tableElement_.draggingColumn = null;
  }
}
