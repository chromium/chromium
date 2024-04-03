// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DestinationManager} from './data/destination_manager.js';
import {createCustomEvent} from './utils/event_utils.js';
import {Destination} from './utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'destination-dropdown-controller' defines events and event handlers to
 * correctly consume changes from mojo providers and inform the
 * `destination-dropdown` element to update.
 */

export const DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION =
    'destination-dropdown.update-selected-destination';

export class DestinationDropdownController extends EventTarget {
  private destinationManager = DestinationManager.getInstance();

  /**
   * @param eventTracker Passed in by owning element to ensure event handlers
   * lifetime is aligned with element.
   */
  constructor(eventTracker: EventTracker) {
    super();
    eventTracker.add(
        this.destinationManager, DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED,
        (e: Event): void =>
            this.onDestinationManagerActiveDestinationChanged(e));
  }

  // Handles logic for notifying UI to update when destination manager
  // active destination changes.
  private onDestinationManagerActiveDestinationChanged(_event: Event): void {
    this.dispatchEvent(
        createCustomEvent(DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION));
  }

  getSelectedDestination(): Destination|null {
    return this.destinationManager.getActiveDestination();
  }
}


declare global {
  interface HTMLElementEventMap {
    [DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION]: CustomEvent<void>;
  }
}
