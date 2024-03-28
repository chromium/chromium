// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createCustomEvent} from '../utils/event_utils.js';
import {getDestinationProvider} from '../utils/mojo_data_providers.js';
import {Destination, DestinationProvider} from '../utils/print_preview_cros_app_types.js';

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

export const DESTINATION_MANAGER_STATE_CHANGED =
    'destination-manager.state-changed';

export class DestinationManager extends EventTarget {
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
  private destinations: Destination[] = [
    // Digital destinations can be added at creation and will be removed if not
    // supported by policy.
    PDF_DESTINATION,
  ];
  private initialDestinationsLoaded = false;
  private state = DestinationManagerState.NOT_LOADED;

  // Private to prevent additional initialization.
  private constructor() {
    super();

    // Setup mojo data providers.
    this.destinationProvider = getDestinationProvider();

    // Request initial data.
    // TODO(b/323421684): Once all initial fetch completes update has initial
    // destinations and trigger event.
    this.updateState(DestinationManagerState.FETCHING);
    this.destinationProvider.getLocalDestinations().then((): void => {
      this.initialDestinationsLoaded = true;
      this.updateState(DestinationManagerState.LOADED);
    });
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

  // Updates manager state and triggers event if state has actually changed.
  // No event fired if `nextState` matches current state.
  private updateState(nextState: DestinationManagerState): void {
    if (nextState === this.state) {
      return;
    }

    this.state = nextState;
    this.dispatchEvent(createCustomEvent(DESTINATION_MANAGER_STATE_CHANGED));
  }
}

declare global {
  interface HTMLElementEventMap {
    [DESTINATION_MANAGER_STATE_CHANGED]: CustomEvent<void>;
  }
}
