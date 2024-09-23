// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {DESTINATION_MANAGER_SESSION_INITIALIZED, DESTINATION_MANAGER_STATE_CHANGED, DestinationManager} from './data/destination_manager.js';
import {createCustomEvent} from './utils/event_utils.js';

/**
 * @fileoverview
 * 'destination-select-controller' defines events and event handlers to
 * correctly consume changes from mojo providers and inform the
 * `destination-select` element to update.
 */

export const DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED =
    'destination-select.show-loading-ui-changed';

// DestinationSelectController defines functionality used to update the
// `destination-select` element.
export class DestinationSelectController extends EventTarget {
  private destinationManager = DestinationManager.getInstance();

  /**
   * @param eventTracker Passed in by owning element to ensure event handlers
   * lifetime is aligned with element.
   */
  constructor(eventTracker: EventTracker) {
    super();
    eventTracker.add(
        this.destinationManager, DESTINATION_MANAGER_STATE_CHANGED,
        (): void => this.onDestinationManagerStateChanged());
    eventTracker.add(
        this.destinationManager, DESTINATION_MANAGER_SESSION_INITIALIZED,
        (): void => this.onDestinationManagerSessionInitialized());
  }

  // Returns whether destination manager has fetched initial destinations and
  // is initialized.
  shouldShowLoadingUi(): boolean {
    return !this.destinationManager.isSessionInitialized() ||
        !this.destinationManager.hasAnyDestinations();
  }

  // Handles notifying UI to update when destination manager
  // state changes.
  private onDestinationManagerStateChanged(): void {
    this.dispatchEvent(
        createCustomEvent(DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED));
  }

  // Handles notifying UI to update when destination manager
  // initialized state changes.
  private onDestinationManagerSessionInitialized(): void {
    this.dispatchEvent(
        createCustomEvent(DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED));
  }
}

declare global {
  interface HTMLElementEventMap {
    [DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED]: CustomEvent<void>;
  }
}
