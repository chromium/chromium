// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeDestinationProvider} from '../fakes/fake_destination_provider.js';
import {Capabilities, Destination, DestinationProviderCompositeInterface, FakeDestinationObserverInterface, PrinterType} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'destination_provider_composite' provides a temporary structure to support
 * the mojo implementation of the DestinationProvider mojom interface combined
 * with fake implementations until all methods can be mojo implemented.
 */

export class DestinationProviderComposite implements
    DestinationProviderCompositeInterface {
  readonly fakeDestinationProvider: FakeDestinationProvider =
      new FakeDestinationProvider();

  fetchCapabilities(destinationId: string, printerType: PrinterType):
      Promise<Capabilities> {
    return this.fakeDestinationProvider.fetchCapabilities(
        destinationId, printerType);
  }

  getLocalDestinations(): Promise<Destination[]> {
    return this.fakeDestinationProvider.getLocalDestinations();
  }

  observeDestinationChanges(observer: FakeDestinationObserverInterface): void {
    this.fakeDestinationProvider.observeDestinationChanges(observer);
  }
}
