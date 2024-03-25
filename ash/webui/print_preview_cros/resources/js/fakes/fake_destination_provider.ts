// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {assert} from 'chrome://resources/js/assert.js';

import {Destination, DestinationProvider} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'fake_destination_provider' is a mock implementation of the
 * `DestinationProvider` mojo interface.
 */

export const GET_LOCAL_DESTINATIONS_METHOD = 'getLocalDestinations';
export const FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY = [];

// Fake implementation of the DestinationProvider for tests and UI.
export class FakeDestinationProvider implements DestinationProvider {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private callCount: Map<string, number> = new Map<string, number>();
  private testDelayMs = 0;

  constructor() {
    this.registerMethods();
  }

  private registerMethods() {
    this.methods.register(GET_LOCAL_DESTINATIONS_METHOD);
    this.methods.setResult(
        GET_LOCAL_DESTINATIONS_METHOD,
        FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY);
    this.callCount.set(GET_LOCAL_DESTINATIONS_METHOD, 0);
  }

  // Handles restoring state of fake to initial state.
  reset(): void {
    this.callCount.clear();
    this.methods = new FakeMethodResolver();
    this.registerMethods();
    this.testDelayMs = 0;
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
    const prevCallCount =
        this.callCount.get(GET_LOCAL_DESTINATIONS_METHOD) ?? 0;
    this.callCount.set(GET_LOCAL_DESTINATIONS_METHOD, prevCallCount + 1);
    return this.methods.resolveMethodWithDelay(
        GET_LOCAL_DESTINATIONS_METHOD, this.testDelayMs);
  }
}
