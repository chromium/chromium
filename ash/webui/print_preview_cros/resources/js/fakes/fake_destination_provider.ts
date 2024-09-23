// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.js';

import {DestinationProviderInterface} from '../../destination_provider.mojom-webui.js';
import {PrinterType} from '../../print.mojom-webui.js';
import {Capabilities, CollateCapability, ColorCapability, ColorOption, ColorType, CopiesCapability, DpiCapability, DpiOption, DuplexCapability, DuplexOption, DuplexType, MediaSizeCapability, MediaSizeOption, MediaTypeCapability, MediaTypeOption, PageOrientation, PageOrientationCapability, PageOrientationOption, PinCapability} from '../../printer_capabilities.mojom-webui.js';
import {Destination, DestinationProvider, FakeDestinationObserverInterface} from '../utils/print_preview_cros_app_types.js';

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
export class FakeDestinationProvider implements DestinationProvider,
                                                DestinationProviderInterface {
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
        {destinations: FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY});
    this.callCount.set(GET_LOCAL_DESTINATIONS_METHOD, 0);
    this.methods.register(FETCH_CAPABILITIES_METHOD);
    this.callCount.set(FETCH_CAPABILITIES_METHOD, 0);
    this.methods.setResult(
        FETCH_CAPABILITIES_METHOD, getFakeCapabilitiesResponse());
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

  getLocalDestinations(): Promise<{destinations: Destination[]}> {
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
    this.methods.setResult(
        GET_LOCAL_DESTINATIONS_METHOD, {destinations: destinations});
  }

  fetchCapabilities(_destinationId: string, _printerType: PrinterType):
      Promise<{capabilities: Capabilities}> {
    this.incrementCallCount(FETCH_CAPABILITIES_METHOD);
    return this.methods.resolveMethodWithDelay(
        FETCH_CAPABILITIES_METHOD, this.testDelayMs);
  }

  setCapabilities(capabilities: Capabilities): void {
    this.methods.setResult(FETCH_CAPABILITIES_METHOD, {capabilities});
  }
}

// TODO(b/323421684): Move this function to "fake_data.ts" once all the
//    DestinationProvider methods are migrated to mojo.
export function getFakeCapabilitiesResponse(destinationId: string = 'Printer1'):
    {capabilities: Capabilities} {
  const collate: CollateCapability = {
    valueDefault: true,
  };

  const color: ColorCapability = {
    options: [
      {
        type: ColorType.kStandardColor,
        vendorId: '1',
        isDefault: true,
      } as ColorOption,
      {
        type: ColorType.kStandardMonochrome,
        vendorId: '2',
      } as ColorOption,
    ],
    resetToDefault: false,
  };

  const copies: CopiesCapability = {
    valueDefault: 1,
    max: 9999,
  };

  const duplex: DuplexCapability = {
    options: [
      {
        type: DuplexType.kNoDuplex,
        isDefault: true,
      } as DuplexOption,
      {
        type: DuplexType.kLongEdge,
      } as DuplexOption,
      {
        type: DuplexType.kShortEdge,
      } as DuplexOption,
    ],
  };

  const pageOrientation: PageOrientationCapability = {
    options: [
      {
        option: PageOrientation.kPortrait,
        isDefault: true,
      } as PageOrientationOption,
      {
        option: PageOrientation.kLandscape,
      } as PageOrientationOption,
      {
        option: PageOrientation.kAuto,
      } as PageOrientationOption,
    ],
    resetToDefault: false,
  };

  const mediaSize: MediaSizeCapability = {
    options: [
      {
        vendorId: 'na_govt-letter_8x10in',
        heightMicrons: 254000,
        widthMicrons: 203200,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 200200,
        imageableAreaTopMicrons: 251000,
        hasBorderlessVariant: true,
        customDisplayName: '8 x 10 in',
        name: 'NA_GOVT_LETTER',
      } as MediaSizeOption,
      {
        vendorId: 'na_legal_8.5x14in',
        heightMicrons: 297000,
        widthMicrons: 210000,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 207000,
        imageableAreaTopMicrons: 294000,
        hasBorderlessVariant: true,
        customDisplayName: 'A4',
        name: 'ISO_A4',
      } as MediaSizeOption,
      {
        vendorId: 'na_legal_8.5x14in',
        heightMicrons: 355600,
        widthMicrons: 215900,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 212900,
        imageableAreaTopMicrons: 352600,
        customDisplayName: 'Legal',
        name: 'NA_LEGAL',
      } as MediaSizeOption,
      {
        vendorId: 'na_letter_8.5x11in',
        heightMicrons: 279400,
        widthMicrons: 215900,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 212900,
        imageableAreaTopMicrons: 276400,
        hasBorderlessVariant: true,
        customDisplayName: 'Letter',
        name: 'NA_LETTER',
      } as MediaSizeOption,
    ],
    resetToDefault: false,
  };

  const mediaType: MediaTypeCapability = {
    options: [
      {
        vendorId: 'stationery',
        customDisplayName: 'Paper (Plain)',
        isDefault: true,
      } as MediaTypeOption,
      {
        vendorId: 'photographic',
        customDisplayName: 'Photo',
      } as MediaTypeOption,
      {
        vendorId: 'envelope',
        customDisplayName: 'Envelope',
      } as MediaTypeOption,
    ],
    resetToDefault: false,
  };

  const dpi: DpiCapability = {
    options: [
      {
        horizontalDpi: 300,
        verticalDpi: 300,
        isDefault: true,
      } as DpiOption,
      {
        horizontalDpi: 600,
        verticalDpi: 600,
      } as DpiOption,
      {
        horizontalDpi: 800,
        verticalDpi: 1000,
      } as DpiOption,
    ],
    resetToDefault: false,
  };

  const pin: PinCapability = {
    supported: false,
  };

  const capabilities: Capabilities = {
    destinationId: destinationId,
    collate: collate,
    color: color,
    copies: copies,
    duplex: duplex,
    pageOrientation: pageOrientation,
    mediaSize: mediaSize,
    mediaType: mediaType,
    dpi: dpi,
    pin: pin,
  };

  return {capabilities: capabilities};
}
