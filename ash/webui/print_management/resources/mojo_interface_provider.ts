// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrintingMetadataProvider, PrintingMetadataProviderInterface} from './printing_manager.mojom-webui.js';

let metadataProvider: PrintingMetadataProviderInterface|null = null;

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
