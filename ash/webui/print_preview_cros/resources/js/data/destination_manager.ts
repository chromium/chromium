// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {createCustomEvent} from '../utils/event_utils.js';
import {getDestinationProvider} from '../utils/mojo_data_providers.js';
import {Destination, DestinationProvider, FakeDestinationObserverInterface, SessionContext, type UiManagedDestinationFields} from '../utils/print_preview_cros_app_types.js';

import {PDF_DESTINATION} from './destination_constants.js';

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
    DestinationManager.instance = null;
  }

  // Non-static properties:
  private destinationProvider: DestinationProvider;
  private destinations: Destination[] = [];
  // Cache used for constant lookup of destinations by key.
  private destinationCache: Map<string, Destination> = new Map();
  private activeDestinationId: string = '';
  private initialDestinationsLoaded = false;
  private state = DestinationManagerState.NOT_LOADED;
  private sessionContext: SessionContext;

  // `initializeSession` is only intended to be called once from the
  // `PrintPreviewCrosAppController`.
  // TODO(b/323421684): Uses session context to configure destination manager
  // with session immutable state such as policy information and primary user.
  initializeSession(sessionContext: SessionContext): void {
    assert(
        !this.sessionContext, 'SessionContext should only be configured once');
    this.sessionContext = sessionContext;
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

  // TODO(b/323421684): Returns true if initial fetch has returned
  // and there are valid destinations available in the destination
  // cache.
  hasLoadedAnInitialDestination(): boolean {
    return this.initialDestinationsLoaded;
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

    // Ensure fields managed by UI values are maintained.
    this.overrideUiManagedFields(destination, existingDestination);

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
    // TODO(b/323421684): Once the initial local destinations fetch completes
    // update has initial destination set, determine relevant initial
    // destination, and create the initial print ticket. If policy restricts
    // fetching a destination type an empty destination list will be returned.
    this.destinationProvider.getLocalDestinations().then(
        (destinations: Destination[]): void => {
          this.addOrUpdateDestinations(destinations);
          this.initialDestinationsLoaded = true;
          this.updateActiveDestination(PDF_DESTINATION.id);
          this.updateState(DestinationManagerState.LOADED);
        });
  }

  // Insert hard-coded digital destinations into set of known destinations.
  // Function should only be called once per session.
  private insertDigitalDestinations(): void {
    assert(!this.destinationCache.get(PDF_DESTINATION.id));
    this.addOrUpdateDestination(PDF_DESTINATION);
  }

  // Creates a merge of `destination` and UI managed fields from `uiFields`
  // to ensure fields set by UI are not lost during update.
  // Example field: `printerManuallySelected`.
  private overrideUiManagedFields(
      destination: Destination, uiFields: UiManagedDestinationFields): void {
    destination.printerManuallySelected = uiFields.printerManuallySelected;
  }

  // Updates destination ID and triggers event.
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
}

declare global {
  interface HTMLElementEventMap {
    [DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED]: CustomEvent<void>;
    [DESTINATION_MANAGER_DESTINATIONS_CHANGED]: CustomEvent<void>;
    [DESTINATION_MANAGER_SESSION_INITIALIZED]: CustomEvent<void>;
    [DESTINATION_MANAGER_STATE_CHANGED]: CustomEvent<void>;
  }
}
