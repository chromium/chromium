// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import {assert} from 'chrome://resources/js/assert.js';

import {createCustomEvent} from '../utils/event_utils.js';
import {getDestinationProvider} from '../utils/mojo_data_providers.js';
import {Destination, DestinationProviderCompositeInterface, FakeDestinationObserverInterface, SessionContext} from '../utils/print_preview_cros_app_types.js';
import {isValidDestination} from '../utils/validation_utils.js';

import {PDF_DESTINATION} from './destination_constants.js';
import {PRINT_TICKET_MANAGER_TICKET_CHANGED, PrintTicketManager} from './print_ticket_manager.js';

/**
 * @fileoverview
 * 'destination_manager' responsible for storing data related to available print
 * destinations as well as signaling updates to subscribed listeners. Manager
 * follows the singleton design pattern to enable access to a shared instance
 * across the app.
 */

export enum DestinationManagerState {
  // Default initial state before destination manager has attempted to fetch
  // destinations.
  NOT_LOADED,
  // Fetch for initial destinations in progress.
  FETCHING,
  // Initial destinations loaded.
  LOADED,
}

export const DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED =
    'destination-manager.active-destination-changed';
export const DESTINATION_MANAGER_DESTINATIONS_CHANGED =
    'destination-manager.destinations-changed';
export const DESTINATION_MANAGER_SESSION_INITIALIZED =
    'destination-manager.session-initialized';
export const DESTINATION_MANAGER_STATE_CHANGED =
    'destination-manager.state-changed';

export class DestinationManager extends EventTarget implements
    FakeDestinationObserverInterface {
  private static instance: DestinationManager|null = null;

  static getInstance(): DestinationManager {
    if (DestinationManager.instance === null) {
      DestinationManager.instance = new DestinationManager();
    }

    return DestinationManager.instance;
  }

  static resetInstanceForTesting(): void {
    DestinationManager.instance?.eventTracker.removeAll();
    DestinationManager.instance = null;
  }

  // Non-static properties:
  private destinationProvider: DestinationProviderCompositeInterface;
  private destinations: Destination[] = [];
  // Cache used for constant lookup of destinations by key.
  private destinationCache: Map<string, Destination> = new Map();
  private activeDestinationId: string = '';
  private initialDestinationsLoaded = false;
  private state = DestinationManagerState.NOT_LOADED;
  private sessionContext: SessionContext;
  private eventTracker = new EventTracker();
  // Managers need to be set after construction to avoid circular dependencies.
  private printTicketManager: PrintTicketManager;

  // `initializeSession` is only intended to be called once from the
  // `PrintPreviewCrosAppController`.
  // TODO(b/323421684): Uses session context to configure destination manager
  // with session immutable state such as policy information and primary user.
  initializeSession(sessionContext: SessionContext): void {
    assert(
        !this.sessionContext, 'SessionContext should only be configured once');
    this.sessionContext = sessionContext;

    // Setup event listeners.
    this.printTicketManager = PrintTicketManager.getInstance();
    this.eventTracker.add(
        this.printTicketManager, PRINT_TICKET_MANAGER_TICKET_CHANGED,
        () => this.onPrintTicketChanged());

    this.fetchInitialDestinations();
    this.dispatchEvent(
        createCustomEvent(DESTINATION_MANAGER_SESSION_INITIALIZED));
  }

  // Returns true only after the initializeSession function has been called with
  // a valid SessionContext.
  isSessionInitialized(): boolean {
    return !!this.sessionContext;
  }

  // Private to prevent additional initialization.
  private constructor() {
    super();

    // Setup mojo data providers.
    this.destinationProvider = getDestinationProvider();
    this.destinationProvider.observeDestinationChanges(this);

    // Digital destinations can be added at creation and will be removed during
    // session initialization if not supported by policy.
    this.insertDigitalDestinations();
  }

  // Returns true if destination exists in cache.
  destinationExists(destinationId: string): boolean {
    return this.destinationCache.has(destinationId);
  }

  // Returns true if initial fetch has returned and there are valid destinations
  // available.
  hasAnyDestinations(): boolean {
    return this.isSessionInitialized() && this.initialDestinationsLoaded &&
        this.destinations.length > 0;
  }

  // Retrieve destination by ID.
  getDestination(destinationId: string): Destination {
    assert(this.destinationExists(destinationId));
    return this.destinationCache.get(destinationId)!;
  }

  // Retrieve a list of all known destinations.
  getDestinations(): Destination[] {
    return this.destinations;
  }

  getState(): DestinationManagerState {
    return this.state;
  }

  // Retrieve active/selected destination or null if the activeDestination ID
  // cannot be found.
  getActiveDestination(): Destination|null {
    // Return early if active destination has not been initialized yet.
    if (this.activeDestinationId === '') {
      return null;
    }

    const active = this.destinationCache.get(this.activeDestinationId);
    assert(active);
    return active;
  }

  // FakeDestinationObserverInterface:
  // `onDestinationsChanged` receives new and updated destinations from the
  // the DestinationProvider then processes the destinations into the set of
  // known destinations. Existing destinations will not be removed from the set
  // of known destinations if disconnected during a preview session.
  onDestinationsChanged(destinations: Destination[]): void {
    this.addOrUpdateDestinations(destinations);
  }

  // Handles processing multiple destinations and triggering the destinations
  // changed event. If the destination list is empty the event is not fired.
  private addOrUpdateDestinations(destinations: Destination[]): void {
    if (destinations.length === 0) {
      // TODO(b/323421684): Check if no-destination state has occurred.
      return;
    }

    destinations.forEach(
        (destination: Destination): void =>
            this.addOrUpdateDestination(destination));
    this.dispatchEvent(
        createCustomEvent(DESTINATION_MANAGER_DESTINATIONS_CHANGED));
  }

  // Inserts new destinations into destination list and cache. If destination
  // is already in cache then update list and cache with merged destination to
  // ensure fields set by UI are not lost.
  private addOrUpdateDestination(destination: Destination): void {
    const existingDestination = this.destinationCache.get(destination.id);
    // First time seeing destination.
    if (!existingDestination) {
      this.destinationCache.set(destination.id, destination);
      this.destinations.push(destination);
      return;
    }

    // Update destination in list and cache.
    const index = this.destinations.findIndex(
        (d: Destination) => d.id === destination.id);
    assert(index !== -1);
    this.destinationCache.set(destination.id, destination);
    this.destinations[index] = destination;
  }

  // Requests destinations from backend and updates manager state to `FETCHING`.
  // Once destinations have been stored, the state is updated to `LOADED` and
  // attempts to select an initial destination.
  private fetchInitialDestinations(): void {
    assert(this.isSessionInitialized);
    // Request initial data.
    this.updateState(DestinationManagerState.FETCHING);
    this.destinationProvider.getLocalDestinations().then(
        (response: {destinations: Destination[]}): void => {
          this.addOrUpdateDestinations(response.destinations);
          this.initialDestinationsLoaded = true;
          // TODO(b/323421684): Refactor selectInitialDestination to call
          // setPrintTicketDestination print ticket manager.
          this.selectInitialDestination();
          this.updateState(DestinationManagerState.LOADED);
        });
  }

  // Insert hard-coded digital destinations into set of known destinations.
  // Function should only be called once per session.
  private insertDigitalDestinations(): void {
    assert(!this.destinationCache.get(PDF_DESTINATION.id));
    this.addOrUpdateDestination(PDF_DESTINATION);
  }

  // Handles active destination updates triggered by the UI. If update is to a
  // different property (active destination already matches the print ticket)
  // do not attempt to update the active destination.
  private onPrintTicketChanged(): void {
    assert(this.printTicketManager);
    const currentPrintTicket = this.printTicketManager.getPrintTicket();
    assert(currentPrintTicket);
    const nextActiveDestinationId = currentPrintTicket.destinationId || '';
    if (nextActiveDestinationId === this.activeDestinationId) {
      return;
    }
    assert(
        isValidDestination(nextActiveDestinationId),
        'PrintTicket won\'t be set to an invalid ID');
    this.updateActiveDestination(nextActiveDestinationId);
  }

  // Determines the best fitting active destination from the available
  // destinations. Best fitting destination is determined in this order:
  //  1. The most recently used available destination from user preferences.
  //  2. Using "matching regex" defined by policy. See DefaultPrinterSelection
  //     policy.
  //  3. Using fallback behavior.
  //  NOTE: CrOS does not support system default printer at this time.
  private selectInitialDestination(): void {
    assert(this.activeDestinationId === '');
    if (this.destinations.length === 0) {
      // TODO(b/323421684): Handle no-destination state.
      return;
    }

    // TODO(b/323421684): Attempt to select a recently used destination.
    // TODO(b/323421684): Attempt to select using policy regex.
    this.selectFallbackDestination();
  }

  // Fallback to PDF destination if available; otherwise use first available
  // destination.
  private selectFallbackDestination(): void {
    assert(this.destinations.length > 0);
    if (this.destinationCache.get(PDF_DESTINATION.id)) {
      this.updateActiveDestination(PDF_DESTINATION.id);
      return;
    }
    this.updateActiveDestination(this.destinations[0].id);
  }

  // Updates active destination ID and triggers event.
  private updateActiveDestination(destinationId: string): void {
    this.activeDestinationId = destinationId;
    this.dispatchEvent(
        createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED));
  }

  // Updates manager state and triggers event if state has actually changed.
  // No event fired if `nextState` matches current state.
  private updateState(nextState: DestinationManagerState): void {
    if (nextState === this.state) {
      return;
    }

    this.state = nextState;
    this.dispatchEvent(createCustomEvent(DESTINATION_MANAGER_STATE_CHANGED));
  }

  // Adds or overrides destination in list and cache.
  setDestinationForTesting(destination: Destination): void {
    this.destinationCache.set(destination.id, destination);
    const index = this.destinations.findIndex(
        (d: Destination) => d.id === destination.id);
    if (index === -1) {
      this.destinations.push(destination);
      return;
    }
    this.destinationCache.set(destination.id, destination);
    this.destinations[index] = destination;
  }

  // Removes destination from list and cache.
  removeDestinationForTesting(destinationId: string): void {
    if (this.destinationCache.delete(destinationId)) {
      const index = this.destinations.findIndex(
          (d: Destination) => d.id === destinationId);
      this.destinations.splice(index);
    }
  }
}

declare global {
  interface HTMLElementEventMap {
    [DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED]: CustomEvent<void>;
    [DESTINATION_MANAGER_DESTINATIONS_CHANGED]: CustomEvent<void>;
    [DESTINATION_MANAGER_SESSION_INITIALIZED]: CustomEvent<void>;
    [DESTINATION_MANAGER_STATE_CHANGED]: CustomEvent<void>;
  }
}
