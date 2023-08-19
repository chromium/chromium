// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrintingMetadataProvider, PrintingMetadataProviderInterface, PrintManagementHandler, PrintManagementHandlerInterface} from './printing_manager.mojom-webui.js';

let metadataProvider: PrintingMetadataProviderInterface|null = null;
let pageHandler: PrintManagementHandlerInterface|null = null;

export function setMetadataProviderForTesting(
    testProvider: PrintingMetadataProviderInterface): void {
  metadataProvider = testProvider;
}

export function getMetadataProvider(): PrintingMetadataProviderInterface {
  if (metadataProvider) {
    return metadataProvider;
  }
  metadataProvider = PrintingMetadataProvider.getRemote();
  return metadataProvider;
}

export function setPrintManagementHandlerForTesting(
    testHandler: PrintManagementHandlerInterface): void {
  pageHandler = testHandler;
}

export function getPrintManagementHandler(): PrintManagementHandlerInterface {
  if (pageHandler) {
    return pageHandler;
  }

  pageHandler = PrintManagementHandler.getRemote();
  return pageHandler;
}
