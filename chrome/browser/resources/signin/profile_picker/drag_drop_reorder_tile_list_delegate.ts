// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

function isIndexInBetweenStartEnd(
    index: number, start: number, end: number): boolean {
  if (start === -1 || end === -1) {
    return false;
  }

  const lower = Math.min(start, end);
  const higher = Math.max(start, end);

  return lower <= index && index <= higher;
}

// Class tags to use throughout the drag events.
//
// Tag for a tile that is shifted during a drag cycle event.
const SHIFTED_TAG: string = 'shifted';

// Interface to interact with the real underlying tile list that expects to have
// the Drag and Drop functionality.
export interface DraggableTileListInterface {
  // Given an index, returns the HTMLElement corresponding to the draggable
  // tile.
  getDraggableTile(index: number): HTMLElement;
  // Given a draggable tile, return its corresponding index in the tile list.
  getDraggableTileIndex(tile: HTMLElement): number;
}

// This delegate class allows any Polymer list container of tiles 'T' to add
// the drag and drop with reordering functionality.
//
// The events that will be redirected to this delegate are:
// - 'dragstart': triggered once when a tile is initially being dragged.
// - 'dragenter': triggered whenever hovering over a tile with the dragging
// tile.
// - 'dragend': triggered once when a tile drag stops, after a drop.
// A full drag event cycle starts with 'dragstart' and ends with 'dragend'.
//
// To activate the drag and drop functionality, a call to
// `initialize()` (only once) will attach all necessary 'drag-' event listeners
// to the proper tiles. This method must be called once the HTML tiles, that are
// intended to be drag and dropped, are properly rendered.
export class DragDropReorderTileListDelegate {
  private tileListInterface_: DraggableTileListInterface;
  private tileCount_;
  private transitionDuration_: number;  // Unit: ms.

  private initialized_ = false;
  private isDragEnabled_ = true;

  private draggingTile_: HTMLElement|null = null;
  private dragStartIndex_: number = -1;
  private dropTargetIndex: number = -1;

  private eventTracker_: EventTracker;

  // ---------------------------------------------------------------------------
  // public section:

  constructor(
      tileListInterface: DraggableTileListInterface, tileCount: number,
      transitionDuration: number = 300) {
    this.tileListInterface_ = tileListInterface;
    this.tileCount_ = tileCount;
    this.transitionDuration_ = transitionDuration;

    this.eventTracker_ = new EventTracker();
  }

  // Initialize tiles to be able to react to drag events and shift with
  // transition effect based on the 'transform' property.
  // Expected to be called once so that a single event of each type is added to
  // the tiles.
  initializeListeners() {
    assert(!this.initialized_);
    this.initialized_ = true;

    for (let i = 0; i < this.tileCount_; ++i) {
      const tile = this.getDraggableTile_(i);
      tile.draggable = true;

      this.eventTracker_.add(tile, 'dragstart', (event: DragEvent) => {
        this.onDragStart_(event);
      });

      this.eventTracker_.add(tile, 'dragenter', (event: DragEvent) => {
        this.onDragEnter_(event);
      });

      // TODO(http://crbug/1466146): check if this event delay can be removed
      // for MacOS. It is making the drop have an awkward movement.
      this.eventTracker_.add(tile, 'dragend', (event: DragEvent) => {
        this.onDragEnd_(event);
      });

      tile.style.transitionProperty = 'transform';
      tile.style.transitionTimingFunction = 'ease-in-out';
      tile.style.transitionDuration = `${this.transitionDuration_}ms`;
    }
  }

  // Clear all drag events listeners and reset tiles drag state.
  clearListeners() {
    if (!this.initialized_) {
      return;
    }

    for (let i = 0; i < this.tileCount_; ++i) {
      const tile = this.getDraggableTile_(i);
      tile.draggable = false;

      tile.style.removeProperty('transitionProperty');
      tile.style.removeProperty('transitionTimingFunction');
      tile.style.removeProperty('transitionDuration');
    }

    this.eventTracker_.removeAll();
  }

  // Toggle the dragging properties of the tiles on or off.
  // This could be useful to temporarily turning off the functionality (e.g.
  // when hovering over some editable elements that are part of a draggable
  // tile).
  toggleDrag(toggle: boolean) {
    assert(this.initialized_);
    this.isDragEnabled_ = toggle;
  }

  // ---------------------------------------------------------------------------
  // private section

  // Event 'dragstart' is applied on the tile that will be dragged. We store the
  // tile being dragged in temporary member variables that will be used
  // throughout a single drag event cycle. We need to store information in
  // member variables since future events will be triggered in different stack
  // calls.
  private onDragStart_(event: DragEvent) {
    if (!this.isDragEnabled_) {
      event.preventDefault();
      return;
    }

    // 'event.target' corresponds to the tile being dragged. Implicit cast to
    // an HTMLElement.
    const tile = event.target as HTMLElement;
    this.markDraggingTile_(tile);

    // `event.dataTransfer` is null in tests.
    if (event.dataTransfer) {
      const pos = tile.getBoundingClientRect();
      // Make the dragging image the tile itself so that reaction to any sub
      // element that is also draggable shows the whole tile being dragged and
      // not the sub element only (e.g. images).
      event.dataTransfer.setDragImage(
          tile, event.x - pos.left, event.y - pos.top);
    }
  }

  // Event 'dragenter' is applied on the tile that is being hovered over.
  // We shift all tiles between the initial dragging tile and the one that we
  // just entered which will create the reordering functionality.
  private onDragEnter_(event: DragEvent) {
    // Check that this event was triggered as part of the tile drag event cycle,
    // having a valid tile that is being dragged, otherwise we discard this
    // event.
    if (this.draggingTile_ === null) {
      return;
    }

    event.preventDefault();

    // Tile that the dragging tile entered.
    const enteredTile = event.target as HTMLElement;

    const newDragTargetIndex = this.computeNewTargetIndex_(enteredTile);

    // Reset any tile that shifted to its initial position, except the tiles
    // that do not need to be shifted back based on the new drag target index.
    this.resetShiftedTiles_(newDragTargetIndex);

    // Set the new drag target index for future drag enter events.
    this.dropTargetIndex = newDragTargetIndex;

    // Increment of +/-1 depending on the direction of the dragging event.
    const indexIncrement =
        Math.sign(this.dragStartIndex_ - this.dropTargetIndex);
    // Loop from target to start with the direction increment.
    // Shift all tiles by 1 spot based on the direction.
    for (let i = this.dropTargetIndex; i !== this.dragStartIndex_;
         i += indexIncrement) {
      const tileToShift = this.getDraggableTile_(i);
      const tileAtTargetLocation = this.getDraggableTile_(i + indexIncrement);
      this.shiftTile_(tileToShift, tileAtTargetLocation);
    }
  }

  // Event 'dragend` is applied on the tile that was dragged and now dropped. We
  // restore all the temporary member variables to their original state. It is
  // the end of the drag event cycle.
  // TODO: Apply the reordering in this function later.
  private onDragEnd_(event: DragEvent) {
    // The 'event.target' of the 'dragend' event is expected to be the same as
    // the one that started the drag event cycle.
    assert(this.draggingTile_);
    assert(this.draggingTile_ === event.target as HTMLElement);

    this.dropTargetIndex = -1;
    this.resetDraggingTile_();
  }

  // Tile 'tileToShift' will shift to the position of 'tileAtTargetLocation'.
  // The shift happens by applying a transform on the tile. The transition
  // effect is set to 'transform' in the 'initialize()'.
  private shiftTile_(
      tileToShift: HTMLElement, tileAtTargetLocation: HTMLElement) {
    // Tag tile as shifted.
    tileToShift.classList.add(SHIFTED_TAG);

    // Compute relative positions to apply to transform with XY Translation.
    const diffx = tileToShift.offsetLeft - tileAtTargetLocation.offsetLeft;
    const diffy = tileToShift.offsetTop - tileAtTargetLocation.offsetTop;
    tileToShift.style.transform =
        `translateX(${- diffx}px) translateY(${- diffy}px)`;
  }

  // Reset elements from start index until the end index.
  // If some indices are between the start index and the new end index, do not
  // reset these elements as their shifted position should not be modified.
  private resetShiftedTiles_(newDragTargetIndex: number) {
    if (this.dropTargetIndex === -1) {
      return;
    }

    // Increment of +/-1 depending on the direction of the dragging event.
    const indexIncrement =
        Math.sign(this.dragStartIndex_ - this.dropTargetIndex);
    // Loop from target to start with the direction increment.
    for (let i = this.dropTargetIndex; i !== this.dragStartIndex_;
         i += indexIncrement) {
      // Do not reset tiles that have indices between the start and new drag
      // target index since their shift should be kept.
      if (!isIndexInBetweenStartEnd(
              i, this.dragStartIndex_, newDragTargetIndex)) {
        this.resetShiftedTile_(this.getDraggableTile_(i));
      }
    }
  }

  // Resetting a tile that was shifted to it's initial state by clearing the
  // transform.
  private resetShiftedTile_(tile: HTMLElement) {
    tile.style.transform = '';
    tile.classList.remove(SHIFTED_TAG);
  }

  // Compute the new drag target index based on the tile that is being hovered
  // over.
  // In case a tile is shifted by a previous reoredering, it's index is not
  // adapted, therefore we should offset the new target index.
  private computeNewTargetIndex_(enteredTile: HTMLElement): number {
    let newTargetIndex = this.getDraggableTileIndex_(enteredTile);

    // If the tile being dragged over was shifted by a previous reordering,
    // it's index will be shifted by 1, so we need to offset it to get
    // the right index.
    if (enteredTile.classList.contains(SHIFTED_TAG)) {
      const indexIncrement =
          Math.sign(this.dragStartIndex_ - this.dropTargetIndex);
      newTargetIndex += indexIncrement;
    }

    return newTargetIndex;
  }

  // Prepare 'this.draggingTile_' member variable as the dragging tile.
  // It will used throughout each drag event cycle and reset in the
  // `resetDraggingTile_()` method which restore the tile to it's initial state.
  private markDraggingTile_(element: HTMLElement) {
    this.draggingTile_ = element;
    this.draggingTile_.classList.add('dragging');
    this.dragStartIndex_ = this.getDraggableTileIndex_(this.draggingTile_);

    // Apply specific style to hide the tile that is being dragged, making sure
    // only the image that sticks on the mouse pointer to be displayed while
    // dragging. A very low value different than 0 is needed, otherwise the
    // element would be considered invisible and would not react to drag events
    // anymore. A value of '0.001' is enough to simulate the 'invisible' effect.
    this.draggingTile_.style.opacity = '0.001';
  }

  // Restores `this.draggingTile_` to it's initial state.
  private resetDraggingTile_() {
    this.draggingTile_!.style.removeProperty('opacity');

    this.dragStartIndex_ = -1;
    this.draggingTile_!.classList.remove('dragging');
    this.draggingTile_ = null;
  }

  private getDraggableTile_(index: number) {
    return this.tileListInterface_.getDraggableTile(index);
  }

  private getDraggableTileIndex_(tile: HTMLElement): number {
    return this.tileListInterface_.getDraggableTileIndex(tile);
  }
}
