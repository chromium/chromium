// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {DestinationProviderComposite} from '../data/destination_provider_composite.js';
import {PrintPreviewPageHandlerComposite} from '../data/print_preview_page_handler_composite.js';

import {DestinationProviderCompositeInterface} from './print_preview_cros_app_types.js';
import type {PrintPreviewPageHandlerCompositeInterface} from './print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'mojo_data_providers' contains accessors to shared mojo data providers. As
 * well as an override method to be used in tests.
 */

let useFakeProviders: boolean = false;
let printPreviewPageHandler: PrintPreviewPageHandlerCompositeInterface|null =
    null;
let destinationProvider: DestinationProviderCompositeInterface|null = null;

// Returns shared instance of PrintPreviewPageHandler.
export function getPrintPreviewPageHandler():
    PrintPreviewPageHandlerCompositeInterface {
  if (printPreviewPageHandler === null) {
    printPreviewPageHandler = new PrintPreviewPageHandlerComposite();
  }

  assert(printPreviewPageHandler);
  return printPreviewPageHandler;
}

// Returns shared instance of DestinationProvider.
export function getDestinationProvider():
    DestinationProviderCompositeInterface {
  if (destinationProvider === null) {
    destinationProvider = new DestinationProviderComposite(useFakeProviders);
  }

  assert(destinationProvider);
  return destinationProvider;
}

// Set useFakeProviders to true and providers to `null`. Needs to be
// called before provider getters to ensure cached value in controllers and
// managers match fake provider configured for testing.
export function resetProvidersForTesting(): void {
  useFakeProviders = true;
  destinationProvider = null;
  printPreviewPageHandler = null;
}
