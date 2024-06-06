// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DESTINATION_MANAGER_DESTINATIONS_CHANGED, DESTINATION_MANAGER_STATE_CHANGED, DestinationManager} from './data/destination_manager.js';
import {PRINT_REQUEST_FINISHED_EVENT, PRINT_REQUEST_STARTED_EVENT, PrintTicketManager} from './data/print_ticket_manager.js';
import {createCustomEvent} from './utils/event_utils.js';
import {Destination} from './utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'destination-dropdown-controller' defines events and event handlers to
 * correctly consume changes from mojo providers and inform the
 * `destination-dropdown` element to update.
 */

export const DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED =
    'destination-dropdown.dropdown-disabled-changed';
export const DESTINATION_DROPDOWN_UPDATE_DESTINATIONS =
    'destination-dropdown.update-destinations';
export const DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION =
    'destination-dropdown.update-selected-destination';

export class DestinationDropdownController extends EventTarget {
  private destinationManager = DestinationManager.getInstance();
  private printTicketManager = PrintTicketManager.getInstance();

  /**
   * @param eventTracker Passed in by owning element to ensure event handlers
   * lifetime is aligned with element.
   */
  constructor(eventTracker: EventTracker) {
    super();
    eventTracker.add(
        this.destinationManager, DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED,
        (): void => this.onDestinationManagerActiveDestinationChanged());
    eventTracker.add(
        this.destinationManager, DESTINATION_MANAGER_DESTINATIONS_CHANGED,
        (): void => this.onDestinationManagerDestinationsChanged());
    eventTracker.add(
        this.destinationManager, DESTINATION_MANAGER_STATE_CHANGED,
        (): void => this.dispatchDropdownDisabled());
    eventTracker.add(
        this.printTicketManager, PRINT_REQUEST_FINISHED_EVENT,
        (): void => this.dispatchDropdownDisabled());
    eventTracker.add(
        this.printTicketManager, PRINT_REQUEST_STARTED_EVENT,
        (): void => this.dispatchDropdownDisabled());
  }

  // Handles logic of when dropdown control should be disabled. Dropdown control
  // should be disabled if:
  // - Initial destinations have not been loaded.
  // - Print request is in progress.
  shouldDisableDropdown(): boolean {
    return !this.destinationManager.hasAnyDestinations() ||
        this.printTicketManager.isPrintRequestInProgress();
  }

  // Handles logic for updating the active destination in the print ticket.
  // If provided destination is already selected or is not a valid destination
  // then return false. Otherwise call setPrintTicketDestination.
  updateActiveDestination(destinationId: string): boolean {
    return this.printTicketManager.setPrintTicketDestination(destinationId);
  }

  // Notifies UI to re-evaluate disabled state.
  private dispatchDropdownDisabled(): void {
    this.dispatchEvent(
        createCustomEvent(DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED));
  }

  // Handles logic for notifying UI to update when destination manager
  // active destination changes.
  private onDestinationManagerActiveDestinationChanged(): void {
    this.dispatchEvent(
        createCustomEvent(DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION));
  }

  // Handles logic for notifying UI to update when destination manager
  // destinations change.
  private onDestinationManagerDestinationsChanged(): void {
    this.dispatchEvent(
        createCustomEvent(DESTINATION_DROPDOWN_UPDATE_DESTINATIONS));
  }

  getSelectedDestination(): Destination|null {
    return this.destinationManager.getActiveDestination();
  }

  // TODO(b/323421684): Replace with functions to get list of digital and
  // recent destinations.
  getDestinations(): Destination[] {
    return this.destinationManager.getDestinations();
  }
}


declare global {
  interface HTMLElementEventMap {
    [DESTINATION_DROPDOWN_UPDATE_DESTINATIONS]: CustomEvent<void>;
    [DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION]: CustomEvent<void>;
  }
}
