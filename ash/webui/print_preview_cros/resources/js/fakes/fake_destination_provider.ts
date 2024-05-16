// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.js';

import {Capabilities, Destination, DestinationProvider, FakeDestinationObserverInterface, PrinterType} from '../utils/print_preview_cros_app_types.js';

import {getFakeCapabilities} from './fake_data.js';

/**
 * @fileoverview
 * 'fake_destination_provider' is a mock implementation of the
 * `DestinationProvider` mojo interface.
 */

export const GET_LOCAL_DESTINATIONS_METHOD = 'getLocalDestinations';
export const FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY = [];
export const OBSERVE_DESTINATION_CHANGES_METHOD = 'observeDestinationChanges';
export const FETCH_CAPABILITIES_METHOD = 'fetchCapabilities';
const OBSERVABLE_ON_DESTINATIONS_CHANGED_METHOD = 'onDestinationsChanged';

// Fake implementation of the DestinationProvider for tests and UI.
export class FakeDestinationProvider implements DestinationProvider {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private callCount: Map<string, number> = new Map<string, number>();
  private testDelayMs = 0;
  private observables: FakeObservables = new FakeObservables();

  constructor() {
    this.registerMethods();
    this.registerObservables();
  }

  private registerMethods() {
    this.methods.register(GET_LOCAL_DESTINATIONS_METHOD);
    this.methods.setResult(
        GET_LOCAL_DESTINATIONS_METHOD,
        FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY);
    this.callCount.set(GET_LOCAL_DESTINATIONS_METHOD, 0);
    this.methods.register(FETCH_CAPABILITIES_METHOD);
    this.callCount.set(FETCH_CAPABILITIES_METHOD, 0);
    this.methods.setResult(FETCH_CAPABILITIES_METHOD, getFakeCapabilities());
  }

  private registerObservables(): void {
    this.observables.register(OBSERVABLE_ON_DESTINATIONS_CHANGED_METHOD);
    const defaultOnDestinationChanged: Destination[] = [];
    this.setDestinationsChangesData(defaultOnDestinationChanged);
  }

  private incrementCallCount(methodName: string): void {
    const prevCallCount = this.callCount.get(methodName) ?? 0;
    this.callCount.set(methodName, prevCallCount + 1);
  }

  // Handles restoring state of fake to initial state.
  reset(): void {
    this.callCount.clear();
    this.methods = new FakeMethodResolver();
    this.registerMethods();
    this.testDelayMs = 0;
    this.observables = new FakeObservables();
    this.registerObservables();
  }

  getCallCount(method: string): number {
    return this.callCount.get(method) ?? 0;
  }

  setTestDelay(delay: number): void {
    assert(delay >= 0);
    this.testDelayMs = delay;
  }

  getMethodsForTesting(): FakeMethodResolver {
    return this.methods;
  }

  getLocalDestinations(): Promise<Destination[]> {
    this.incrementCallCount(GET_LOCAL_DESTINATIONS_METHOD);
    return this.methods.resolveMethodWithDelay(
        GET_LOCAL_DESTINATIONS_METHOD, this.testDelayMs);
  }

  // Setup observable for `OBSERVABLE_ON_DESTINATIONS_CHANGED_METHOD` to enable
  // testing observer updates when `triggerOnDestinationChanged` called.
  observeDestinationChanges(observer: FakeDestinationObserverInterface): void {
    this.observables.observe(
        OBSERVABLE_ON_DESTINATIONS_CHANGED_METHOD,
        (destinations: Destination[]): void => {
          observer.onDestinationsChanged(destinations);
        });
    this.incrementCallCount(OBSERVE_DESTINATION_CHANGES_METHOD);
  }

  // Set destination list to be returned when observer is called.
  setDestinationsChangesData(destinations: Destination[]) {
    // Parameters from observer functions are returned in an array.
    this.observables.setObservableData(
        OBSERVABLE_ON_DESTINATIONS_CHANGED_METHOD, [[destinations]]);
  }

  // Trigger destination observer function onDestinationsChanged.
  // `observeDestinationChanges` must be called at least once or this function
  // will throw an error.
  triggerOnDestinationsChanged(): void {
    this.observables.trigger(OBSERVABLE_ON_DESTINATIONS_CHANGED_METHOD);
  }

  setLocalDestinationResult(destinations: Destination[]): void {
    this.methods.setResult(GET_LOCAL_DESTINATIONS_METHOD, destinations);
  }

  fetchCapabilities(_destinationId: string, _printerType: PrinterType):
      Promise<Capabilities> {
    this.incrementCallCount(FETCH_CAPABILITIES_METHOD);
    return this.methods.resolveMethodWithDelay(
        FETCH_CAPABILITIES_METHOD, this.testDelayMs);
  }

  setCapabiltiies(capabilities: Capabilities): void {
    this.methods.setResult(FETCH_CAPABILITIES_METHOD, capabilities);
  }
}
