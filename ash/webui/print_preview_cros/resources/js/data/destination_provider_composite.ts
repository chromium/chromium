// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DestinationProvider, DestinationProviderInterface} from '../../destination_provider.mojom-webui.js';
import {PrinterType} from '../../print.mojom-webui.js';
import {Capabilities} from '../../printer_capabilities.mojom-webui.js';
import {FakeDestinationProvider} from '../fakes/fake_destination_provider.js';
import {Destination, DestinationProviderCompositeInterface, FakeDestinationObserverInterface} from '../utils/print_preview_cros_app_types.js';

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
  private destinationProviderInterface: DestinationProviderInterface|null =
      null;

  constructor(useFakeProviders: boolean) {
    if (useFakeProviders) {
      this.destinationProviderInterface = this.fakeDestinationProvider;
      return;
    }

    this.destinationProviderInterface = DestinationProvider.getRemote();
  }

  fetchCapabilities(destinationId: string, printerType: PrinterType):
      Promise<{capabilities: Capabilities}> {
    return this.destinationProviderInterface!.fetchCapabilities(
        destinationId, printerType);
  }

  getLocalDestinations(): Promise<{destinations: Destination[]}> {
    return this.fakeDestinationProvider.getLocalDestinations();
  }

  observeDestinationChanges(observer: FakeDestinationObserverInterface): void {
    this.fakeDestinationProvider.observeDestinationChanges(observer);
  }
}
