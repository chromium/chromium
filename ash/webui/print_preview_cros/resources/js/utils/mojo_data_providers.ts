// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {FakeDestinationProvider} from '../fakes/fake_destination_provider.js';
import {FakePrintPreviewPageHandler} from '../fakes/fake_print_preview_page_handler.js';

import {DestinationProvider, type PrintPreviewPageHandler} from './print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'mojo_data_providers' contains accessors to shared mojo data providers. As
 * well as an override method to be used in tests.
 */

let printPreviewPageHandler: PrintPreviewPageHandler|null = null;
let destinationProvider: DestinationProvider|null = null;

// Returns shared instance of PrintPreviewPageHandler.
export function getPrintPreviewPageHandler(): PrintPreviewPageHandler {
  if (printPreviewPageHandler == null) {
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
  }

  assert(printPreviewPageHandler);
  return printPreviewPageHandler;
}

// Override shared instance of PrintPreviewPageHandle for testing.
export function setPrintPreviewPageHandlerForTesting(
    handler: PrintPreviewPageHandler): void {
  printPreviewPageHandler = handler;
}

// Returns shared instance of DestinationProvider.
export function getDestinationProvider(): DestinationProvider {
  if (destinationProvider == null) {
    destinationProvider = new FakeDestinationProvider();
  }

  assert(destinationProvider);
  return destinationProvider;
}

// Override shared instance of DestinationProvider for testing.
export function setDestinationProviderForTesting(provider: DestinationProvider):
    void {
  destinationProvider = provider;
}
