// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * DragAndDropManager is a class that handles the drag + drop implementation
 * of CustomizeButtonsSubsection and CustomizeButtonRow. Specifically, it
 * handles management of the drag indicator (the line between rows when an item
 * is being dragged over), calculates the drop position, and invokes a callback
 * when an eligible element is dropped.
 */

import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {CustomizeButtonRowElement} from './customize_button_row.js';

enum DropPosition {
  ABOVE = 0,
  BELOW = 1,
}

interface DropDestination {
  element: CustomizeButtonRowElement;
  position: DropPosition;
}

export interface OnDropCallback {
  (originIndex: number, destinationIndex: number): void;
}

const DATA_TRANSFER_KEY = 'settings-data';

/**
 * Mutates the given DragEvent to set data on its dataTransfer property.
 * @param dragEvent The DragEvent to mutate.
 * @param originIndex The index of the row from which the drop originated.
 */
export function setDataTransferOriginIndex(
    dragEvent: DragEvent, originIndex: number): void {
  if (dragEvent.dataTransfer) {
    dragEvent.dataTransfer.setData(
        DATA_TRANSFER_KEY, JSON.stringify({originIndex}));
  }
}

/**
 * Retrieves data from the given DragEvent using its dataTransfer property.
 * @param dragEvent The DragEvent to retrieve data from.
 */
export function getDataTransferOriginIndex(dragEvent: DragEvent): number|null {
  if (!dragEvent.dataTransfer) {
    return null;
  }
  const settingsDataRaw = dragEvent.dataTransfer.getData(DATA_TRANSFER_KEY);
  if (!settingsDataRaw) {
    return null;
  }

  // Use a try/catch in case the JSON parse fails.
  try {
    const settingsData = JSON.parse(settingsDataRaw);
    if (settingsData.originIndex === undefined) {
      return null;
    }
    return settingsData.originIndex;
  } catch (err) {
    return null;
  }
}

export class DragAndDropManager {
  private eventTracker_: EventTracker = new EventTracker();
  private dropDestination: DropDestination|null;
  private dropElement: CustomizeButtonRowElement|null;
  private onDropCallback: OnDropCallback;

  init(element: HTMLElement, onDropCallbackParam: OnDropCallback): void {
    this.eventTracker_.add(
        element, 'dragover', (e: DragEvent) => this.onDragOver_(e));
    this.eventTracker_.add(element, 'dragleave', () => this.onDragLeave_());
    this.eventTracker_.add(element, 'drop', (e: DragEvent) => this.onDrop_(e));
    this.onDropCallback = onDropCallbackParam;
  }

  destroy(): void {
    this.clearDropIndicator_();
    this.eventTracker_.removeAll();
  }

  private onDragOver_(event: DragEvent): void {
    // Preventing default allows the drop event to work later.
    // See MDN docs http://go/mdn/API/HTMLElement/drop_event for more info.
    event.preventDefault();

    this.clearDropIndicator_();

    // Get the element that is being dragged over.
    const overElement = this.getButtonRowElement_(event.composedPath());
    if (!overElement) {
      return;
    }

    this.dropDestination =
        this.calculateDropDestination_(event.clientY, overElement);
    if (!this.dropDestination) {
      this.dropElement = null;
      return;
    }

    this.dropElement = this.dropDestination.element;

    if (this.dropDestination.position === DropPosition.ABOVE) {
      this.dropElement.classList.add('drop-indicator-top');
    } else {
      this.dropElement.classList.add('drop-indicator-bottom');
    }
  }

  private clearDropIndicator_(): void {
    if (this.dropElement) {
      this.dropElement.classList.remove('drop-indicator-top');
      this.dropElement.classList.remove('drop-indicator-bottom');
    }
  }

  private onDragLeave_(): void {
    this.clearDropIndicator_();
  }

  private onDrop_(event: DragEvent): void {
    this.clearDropIndicator_();

    const originIndex = getDataTransferOriginIndex(event);
    if (originIndex === null || !this.dropDestination) {
      return;
    }

    let destinationElementIndex = this.dropDestination.element.remappingIndex;

    // Before modifying the destination index, check if the element is
    // being dropped into the same position.
    if (originIndex === destinationElementIndex) {
      return;
    }

    if (originIndex < destinationElementIndex) {
      // If originIndex is before destinationElementIndex, shift the destination
      // index up by one to account for the origin element being removed.
      destinationElementIndex -= 1;
    }

    if (this.dropDestination.position === DropPosition.BELOW) {
      destinationElementIndex += 1;
    }

    // After resolving the destination index, is the element ending up in the
    // same position?
    if (originIndex === destinationElementIndex) {
      // Element was dropped into the same position.
      return;
    }

    this.onDropCallback(originIndex, destinationElementIndex);
  }

  /**
   * Given a path (from event.composedPath()), this function returns the
   * CustomizeButtonRowElement that was targeted.
   */
  private getButtonRowElement_(path?: EventTarget[]): CustomizeButtonRowElement
      |null {
    if (!path) {
      return null;
    }

    for (let i = 0; i < path.length; i++) {
      const element = path[i] as Element;
      if (element.tagName === CustomizeButtonRowElement.is.toUpperCase()) {
        return path[i] as CustomizeButtonRowElement;
      }
    }
    return null;
  }

  /**
   * This function determines where the drop should occur by using the mouse
   * position during the drag.
   */
  private calculateDropDestination_(
      elementClientY: number,
      overElement: CustomizeButtonRowElement): DropDestination {
    const rect = overElement.getBoundingClientRect();
    const yRatio = (elementClientY - rect.top) / rect.height;

    if (yRatio <= .5) {
      return {element: overElement, position: DropPosition.ABOVE};
    } else {
      return {element: overElement, position: DropPosition.BELOW};
    }
  }
}
