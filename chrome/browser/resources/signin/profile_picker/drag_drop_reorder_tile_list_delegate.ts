// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ProfileState} from './manage_profiles_browser_proxy.js';

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
// Tag for the tile that is being dragged in the drag cycle event.
const DRAGGING_TAG: string = 'dragging';
// Tag for a tile that is done shifting during a drag cycle event.
const SHIFTED_TAG: string = 'shifted';
// Tag for a tile while it is shifting (applying the transition effect).
// This could mean either shifting in (to new position) or shifting back (to
// initial position).
const SHIFTING_TAG: string = 'shifting';

// Interface to interact with the real underlying tile list that expects to have
// the Drag and Drop functionality.
export interface DraggableTileListInterface {
  // Given an index, returns the HTMLElement corresponding to the draggable
  // tile.
  getDraggableTile(index: number): HTMLElement;
  // Given a draggable tile, return its corresponding index in the tile list.
  getDraggableTileIndex(tile: HTMLElement): number;

  // On drag end, send back the indices of the tiles that were affected.
  // initialIndex: is the index of the tile that was dragged.
  // finalIndex: is the index of the tile that was targeted, the location at
  // which the dragging tile was dropped.
  onDragEnd(initialIndex: number, finalIndex: number): void;
}

// This delegate class allows any Lit list container of tiles to add the
// drag and drop with reordering functionality.
//
// The events that will be redirected to this delegate are:
// - 'dragstart': triggered once when a tile is initially being dragged.
// - 'dragenter': triggered whenever hovering over a tile with the dragging
// tile.
// - 'dragover': triggered continuously as long as the dragging tile is over
// another tile.
// - 'dragend': triggered once when a tile drag stops, after a drop.
// A full drag event cycle starts with 'dragstart' and ends with 'dragend'.
//
// The drag and drop functionality will be active once this object is
// initialized, a call to `initializeListeners_()` will attach all necessary
// 'drag-' event listeners to the proper tiles. This instance should be
// constructed once the HTML tiles, that are intended to be drag and dropped,
// are properly rendered.
export class DragDropReorderTileListDelegate {
  private element: CrLitElement;
  private tileList_: ProfileState[];
  private tileListInterface_: DraggableTileListInterface;
  private transitionDuration_: number;  // Unit: ms.

  private isDragEnabled_ = true;

  private isDragging_: boolean = false;
  private draggingTile_: HTMLElement|null = null;
  private dragStartIndex_: number = -1;
  private dropTargetIndex_: number = -1;

  private eventTracker_: EventTracker;

  // ---------------------------------------------------------------------------
  // public section:

  constructor(
      element: CrLitElement, tileList: ProfileState[],
      tileListInterface: DraggableTileListInterface,
      transitionDuration: number = 300) {
    this.element = element;
    this.tileList_ = tileList;
    this.tileListInterface_ = tileListInterface;
    this.transitionDuration_ = transitionDuration;

    this.eventTracker_ = new EventTracker();

    this.initializeListeners_();
  }

  // Clear all drag events listeners and reset tiles drag state.
  clearListeners() {
    for (let i = 0; i < this.tileList_.length; ++i) {
      const tile = this.getDraggableTile_(i);
      tile.draggable = false;
    }

    this.eventTracker_.removeAll();
  }

  // Toggle the dragging properties of the tiles on or off.
  // This could be useful to temporarily turning off the functionality (e.g.
  // when hovering over some editable elements that are part of a draggable
  // tile).
  toggleDrag(toggle: boolean) {
    this.isDragEnabled_ = toggle;
  }

  // ---------------------------------------------------------------------------
  // private section

  // Initialize tiles to be able to react to drag events and shift with
  // transition effect based on the 'transform' property.
  // Expected to be called once so that a single event of each type is added to
  // the tiles.
  private initializeListeners_() {
    for (let i = 0; i < this.tileList_.length; ++i) {
      const tile = this.getDraggableTile_(i);
      tile.draggable = true;

      this.eventTracker_.add(tile, 'dragstart', (event: DragEvent) => {
        this.onDragStart_(event);
      });

      this.eventTracker_.add(tile, 'dragenter', (event: DragEvent) => {
        this.onDragEnter_(event);
      });

      this.eventTracker_.add(tile, 'dragover', (event: DragEvent) => {
        this.onDragOver_(event);
      }, false);

      this.eventTracker_.add(tile, 'dragend', (event: DragEvent) => {
        this.onDragEnd_(event);
      });
    }

    // React to all elements being dragged over. We need this global Lit
    // element listener in order to allow dropping an element on top of another
    // one. For that, we need a call to `preventDefault()` with the proper
    // event/element (it could be any sub-element, potentially not the tile
    // since the tile we are replacing is potentially moved and therefore not
    // triggering any more drag over events that will be associated with the
    // dragend of our drag event cycle of interest).
    // Therefore, we only use this listener to allow properly dropping the tile.
    this.eventTracker_.add(this.element, 'dragover', (event: DragEvent) => {
      // Only react if we are part of our drag event cycle. This event will
      // trigger for any element being dragged over within the Lit
      // element.
      if (!this.isDragging_) {
        return;
      }

      event.preventDefault();
    });
  }

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

    this.isDragging_ = true;
    // 'event.target' corresponds to the tile being dragged. Implicit cast to
    // an HTMLElement.
    const tile = event.target as HTMLElement;
    this.markDraggingTile_(tile);

    // Prepare all tiles transition effects at the beginning of the drag event
    // cycle.
    this.setAllTilesTransitions_();

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
    // Check that this event was triggered as part of the tile drag event cycle
    // otherwise we discard this event.
    if (!this.isDragging_) {
      return;
    }

    event.preventDefault();

    // Tile that the dragging tile entered.
    const enteredTile = event.target as HTMLElement;

    // Do not react to shifting or dragging tile.
    if (enteredTile.classList.contains(SHIFTING_TAG) ||
        enteredTile.classList.contains(DRAGGING_TAG)) {
      return;
    }

    const newDragTargetIndex = this.computeNewTargetIndex_(enteredTile);

    // Reset any tile that shifted to its initial position, except the tiles
    // that do not need to be shifted back based on the new drag target index.
    this.resetShiftedTiles_(newDragTargetIndex);

    // Set the new drag target index for future drag enter events.
    this.dropTargetIndex_ = newDragTargetIndex;

    // Increment of +/-1 depending on the direction of the dragging event.
    const indexIncrement =
        Math.sign(this.dragStartIndex_ - this.dropTargetIndex_);
    // Loop from target to start with the direction increment.
    // Shift all tiles by 1 spot based on the direction.
    for (let i = this.dropTargetIndex_; i !== this.dragStartIndex_;
         i += indexIncrement) {
      const tileToShift = this.getDraggableTile_(i);
      // No need to shift tiles that are already shifted.
      if (tileToShift.classList.contains(SHIFTED_TAG)) {
        continue;
      }

      const tileAtTargetLocation = this.getDraggableTile_(i + indexIncrement);
      this.shiftTile_(tileToShift, tileAtTargetLocation);
    }
  }

  // Event 'dragover' is applied on the tile that is being hovered over, it will
  // be periodically triggered as long as the dragging tile is over a specific
  // tile. We use this event to make sure we do not miss any drag enter event
  // that might have happened while tiles are shifting.
  private onDragOver_(event: DragEvent) {
    // Check that this event was triggered as part of the tile drag event cycle
    // otherwise we discard this event.
    if (!this.isDragging_) {
      return;
    }

    event.preventDefault();

    const overTile = event.target as HTMLElement;
    // Do not react to shifting or dragging tiles.
    if (overTile.classList.contains(SHIFTING_TAG) ||
        overTile.classList.contains(DRAGGING_TAG)) {
      return;
    }

    // If the dragging tile stays over a shifting tile while it is shifting no
    // drag enter event will be called, or a drag enter event can be missed
    // while an element is shifting, so we simulate another drag enter event
    // after the shifting is done.
    this.onDragEnter_(event);
  }

  // Event 'dragend` is applied on the tile that was dragged and now dropped. We
  // restore all the temporary member variables to their original state. It is
  // the end of the drag event cycle.
  // If a valid target index results from the drag events, perform a reordering
  // on the underlying list. Then notify the changes so that they are rendered.
  // Transition effects are disabled at this point not to have back and forth
  // animations.
  private onDragEnd_(event: DragEvent) {
    // The 'event.target' of the 'dragend' event is expected to be the same as
    // the one that started the drag event cycle.
    assert(this.draggingTile_);
    assert(this.draggingTile_ === event.target as HTMLElement);

    this.isDragging_ = false;

    // Reset all the tiles that shifted during the drag events.
    // Disable transition so that the change is instant and re-alligned by the
    // data change.
    this.resetAllTilesWithoutTransition_();

    if (this.dropTargetIndex_ !== -1) {
      // In case a reorder should happen:
      // - Apply the changes on the original list.
      // - The changes will cause a re-rendering of the Lit element which
      // will take into account the changes and have all the tiles at their
      // right place.
      this.applyChanges_();

      // Notfiy the list of the changes.
      this.tileListInterface_.onDragEnd(
          this.dragStartIndex_, this.dropTargetIndex_);
    }

    this.dropTargetIndex_ = -1;
    this.resetDraggingTile_();
  }

  // Tile 'tileToShift' will shift to the position of 'tileAtTargetLocation'.
  // The shift happens by applying a transform on the tile. The transition
  // effect is set to 'transform' in the 'setAllTilesTransitions()'.
  //
  // Shifting a tile steps:
  // - mark the tile as `SHIFTING_TAG`.
  // - apply the corresponding transform.
  // - transition effect happening with a duration of
  // 'this.transitionDuration_'.
  // - delayed function call to switch the tag from `SHIFTING_TAG` to
  // `SHIFTED_TAG`. after the transition effect is done.
  private shiftTile_(
      tileToShift: HTMLElement, tileAtTargetLocation: HTMLElement) {
    // Tag tile as shifted.
    tileToShift.classList.add(SHIFTING_TAG);

    // Increase the 'zIndex' property of SHIFTING and SHIFTED tiles in order to
    // give them priority for drag enter/over events over other elements.
    tileToShift.style.zIndex = '2';

    // Compute relative positions to apply to transform with XY Translation.
    const diffx = tileToShift.offsetLeft - tileAtTargetLocation.offsetLeft;
    const diffy = tileToShift.offsetTop - tileAtTargetLocation.offsetTop;
    tileToShift.style.transform =
        `translateX(${- diffx}px) translateY(${- diffy}px)`;

    const onShiftTransitionEnd = () => {
      tileToShift.ontransitionend = null;

      tileToShift.classList.remove(SHIFTING_TAG);
      // In case the dragging has stopped before the delayed function, we do
      // not want to tag this tile.
      if (this.isDragging_) {
        tileToShift.classList.add(SHIFTED_TAG);
      }
    };

    if (this.transitionDuration_ !== 0) {
      tileToShift.ontransitionend = onShiftTransitionEnd;
    } else {
      // If `this.transitionDuration_` is 0, then we need to perform the ending
      // function directly, as there is no transition happening. This could
      // happen in tests or if the usage requires no transition.
      onShiftTransitionEnd();
    }
  }

  // Reset elements from start index until the end index.
  // If some indices are between the start index and the new end index, do not
  // reset these elements as their shifted position should not be modified.
  private resetShiftedTiles_(newDragTargetIndex: number) {
    if (this.dropTargetIndex_ === -1) {
      return;
    }

    // Increment of +/-1 depending on the direction of the dragging event.
    const indexIncrement =
        Math.sign(this.dragStartIndex_ - this.dropTargetIndex_);
    // Loop from target to start with the direction increment.
    for (let i = this.dropTargetIndex_; i !== this.dragStartIndex_;
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
  // - Transition effect happening while the tile is shifting back.
  // - delayed function call to remove the `SHIFTING_TAG` tag after the
  // transition is done.
  private resetShiftedTile_(tileToShiftBack: HTMLElement) {
    tileToShiftBack.style.transform = '';
    tileToShiftBack.classList.remove(SHIFTED_TAG);
    tileToShiftBack.classList.add(SHIFTING_TAG);

    const onShiftBackTransitionEnd = () => {
      tileToShiftBack.ontransitionend = null;

      // Reset previously increased 'zIndex'.
      tileToShiftBack.style.removeProperty('z-index');

      tileToShiftBack.classList.remove(SHIFTING_TAG);
      // Can potentially be added if the shift back happens before the end
      // of the initial shift.
      tileToShiftBack.classList.remove(SHIFTED_TAG);
    };

    if (this.transitionDuration_ !== 0) {
      tileToShiftBack.ontransitionend = onShiftBackTransitionEnd;
    } else {
      // If `this.transitionDuration_` is 0, then we need to perform the ending
      // function directly, as there is no transition happening. This could
      // happen in tests or if the usage requires no transition.
      onShiftBackTransitionEnd();
    }
  }

  // Apply changes on the underlying tile list through the Lit element by
  // performing two splices, the changes applied will cause a re-rendering of
  // the Lit element.
  private applyChanges_() {
    // Remove the dragging tile from its original index.
    const [draggingTile] = this.tileList_.splice(this.dragStartIndex_, 1);
    assert(draggingTile);
    // Place it on the target index.
    this.tileList_.splice(this.dropTargetIndex_, 0, draggingTile);

    this.element.requestUpdate();
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
          Math.sign(this.dragStartIndex_ - this.dropTargetIndex_);
      newTargetIndex += indexIncrement;
    }

    return newTargetIndex;
  }

  // Prepare 'this.draggingTile_' member variable as the dragging tile.
  // It will used throughout each drag event cycle and reset in the
  // `resetDraggingTile_()` method which restore the tile to it's initial state.
  // Notifies the tile that the drag event started to allow for UI
  // modifications.
  private markDraggingTile_(element: HTMLElement) {
    this.draggingTile_ = element;
    this.draggingTile_.classList.add(DRAGGING_TAG);
    this.dragStartIndex_ = this.getDraggableTileIndex_(this.draggingTile_);

    // Apply specific style to hide the tile that is being dragged, making sure
    // only the image that sticks on the mouse pointer to be displayed while
    // dragging. A very low value different than 0 is needed, otherwise the
    // element would be considered invisible and would not react to drag events
    // anymore. A value of '0.001' is enough to simulate the 'invisible' effect.
    this.draggingTile_.style.opacity = '0.001';

    this.draggingTile_.dispatchEvent(new Event('drag-tile-start'));
  }

  // Restores `this.draggingTile_` to it's initial state.
  private resetDraggingTile_() {
    this.draggingTile_!.style.removeProperty('opacity');

    this.dragStartIndex_ = -1;
    this.draggingTile_!.classList.remove(DRAGGING_TAG);
    this.draggingTile_ = null;
  }

  // Clear all tiles transition effects, and remove the temporary transforms on
  // all tiles that shifted or are shifting.
  private resetAllTilesWithoutTransition_() {
    // Reset all tiles potential transform values or shited/shifting values.
    // Also clear all tiles transition effects so that the repositioning doesn't
    // animate.
    for (let i = 0; i < this.tileList_.length; ++i) {
      const tile = this.getDraggableTile_(i);
      tile.classList.remove(SHIFTED_TAG);
      tile.classList.remove(SHIFTING_TAG);
      tile.style.removeProperty('transition');
      tile.style.removeProperty('transform');
      tile.style.removeProperty('z-index');
    }
  }

  // Set all the tiles transition values. Transition will happen when the
  // transform property is changed using the `this.transitionDuration_` value
  // set at construction.
  private setAllTilesTransitions_() {
    for (let i = 0; i < this.tileList_.length; ++i) {
      const tile = this.getDraggableTile_(i);
      tile.style.transition =
          `transform ease-in-out ${this.transitionDuration_}ms`;
    }
  }

  private getDraggableTile_(index: number) {
    return this.tileListInterface_.getDraggableTile(index);
  }

  private getDraggableTileIndex_(tile: HTMLElement): number {
    return this.tileListInterface_.getDraggableTileIndex(tile);
  }
}
