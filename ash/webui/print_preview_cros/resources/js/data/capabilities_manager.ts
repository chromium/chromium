// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {PrinterType} from '../../print.mojom-webui.js';
import {Capabilities} from '../../printer_capabilities.mojom-webui.js';
import {createCustomEvent} from '../utils/event_utils.js';
import {getDestinationProvider} from '../utils/mojo_data_providers.js';
import {DestinationProviderCompositeInterface, SessionContext} from '../utils/print_preview_cros_app_types.js';

import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DestinationManager} from './destination_manager.js';

/**
 * @fileoverview
 * 'capabilties_manager' responsible for requesting and storing the printing
 * capabilities for the active destination.
 */

export const CAPABILITIES_MANAGER_SESSION_INITIALIZED =
    'capabilities-manager.session-initialized';
export const CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_LOADING =
    'capabilities-manager.active-destination-caps-loading';
export const CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_READY =
    'capabilities-manager.active-destination-caps-ready';

export class CapabilitiesManager extends EventTarget {
  private static instance: CapabilitiesManager|null = null;

  static getInstance(): CapabilitiesManager {
    if (CapabilitiesManager.instance === null) {
      CapabilitiesManager.instance = new CapabilitiesManager();
    }

    return CapabilitiesManager.instance;
  }

  static resetInstanceForTesting(): void {
    CapabilitiesManager.instance = null;
  }

  // Non-static properties:
  private destinationProvider: DestinationProviderCompositeInterface|null;
  private sessionContext: SessionContext;
  private eventTracker = new EventTracker();
  private destinationManager: DestinationManager =
      DestinationManager.getInstance();
  private capabilitiesCache = new Map<string, Capabilities>();
  private activeDestinationCapabilitiesLoaded = false;

  // Prevent additional initialization.
  private constructor() {
    super();

    // Setup mojo data providers.
    this.destinationProvider = getDestinationProvider();

    this.eventTracker.add(
        this.destinationManager, DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED,
        (): void => this.fetchCapabilitesForActiveDestination());
  }

  // `initializeSession` is only intended to be called once from the
  // `PrintPreviewCrosAppController`.
  initializeSession(sessionContext: SessionContext): void {
    assert(
        !this.sessionContext, 'SessionContext should only be configured once');
    this.sessionContext = sessionContext;

    this.dispatchEvent(
        createCustomEvent(CAPABILITIES_MANAGER_SESSION_INITIALIZED));
  }

  private fetchCapabilitesForActiveDestination(): void {
    const destination = this.destinationManager.getActiveDestination();
    if (destination === null) {
      return;
    }

    this.setActiveCapabilitiesLoading();

    const cachedCapabilities = this.capabilitiesCache.get(destination.id);
    if (cachedCapabilities) {
      this.setActiveCapabilitiesReady();
      return;
    }

    // TODO(b/323421684): Use printer type from destination once the
    // `Destination` object is mojo typed.
    this.destinationProvider!
        .fetchCapabilities(destination.id, PrinterType.kPdf)
        .then(
            // TODO(b/323421684): Create a CapabilitiesResponse.
            (response: {capabilities: Capabilities}) =>
                this.onCapabilitiesFetched(response.capabilities));
  }

  private onCapabilitiesFetched(caps: Capabilities): void {
    // TODO(b/323421684): Handle failed capabilities call.
    if (!caps) {
      return;
    }

    this.capabilitiesCache.set(caps.destinationId, caps);

    // Since multiple capabilities requests can be in-flight simultaneously,
    // verify this capabilities response belongs to the active destination
    // before sending the ready event.
    const activeDestination = this.destinationManager.getActiveDestination();
    assert(activeDestination);
    if (caps.destinationId === activeDestination.id) {
      this.setActiveCapabilitiesReady();
    }
  }

  // Returns the capabilities from the active destination if available.
  getActiveDestinationCapabilities(): Capabilities|undefined {
    const activeDestination = this.destinationManager.getActiveDestination();
    if (activeDestination === null) {
      return undefined;
    }

    return this.capabilitiesCache.get(activeDestination.id);
  }

  private setActiveCapabilitiesLoading(): void {
    this.activeDestinationCapabilitiesLoaded = false;
    this.dispatchEvent(createCustomEvent(
        CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_LOADING));
  }

  private setActiveCapabilitiesReady(): void {
    this.activeDestinationCapabilitiesLoaded = true;
    this.dispatchEvent(
        createCustomEvent(CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_READY));
  }

  areActiveDestinationCapabilitiesLoaded(): boolean {
    return this.activeDestinationCapabilitiesLoaded;
  }

  // Returns true only after the `initializeSession` function has been called
  // with a valid `SessionContext`.
  isSessionInitialized(): boolean {
    return !!this.sessionContext;
  }
}

declare global {
  interface HTMLElementEventMap {
    [CAPABILITIES_MANAGER_SESSION_INITIALIZED]: CustomEvent<void>;
    [CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_LOADING]: CustomEvent<void>;
    [CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_READY]: CustomEvent<void>;
  }
}
