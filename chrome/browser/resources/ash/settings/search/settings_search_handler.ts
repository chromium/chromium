// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides functions used for OS settings search.
 * Also provides a way to inject a test implementation for verifying
 * OS settings search.
 */

import {SearchHandler, SearchHandlerInterface} from '../mojom-webui/search.mojom-webui.js';

let settingsSearchHandler: SearchHandlerInterface|null = null;

export function setSettingsSearchHandlerForTesting(
    testSearchHandler: SearchHandlerInterface): void {
  settingsSearchHandler = testSearchHandler;
}

export function getSettingsSearchHandler(): SearchHandlerInterface {
  if (settingsSearchHandler) {
    return settingsSearchHandler;
  }

  settingsSearchHandler = SearchHandler.getRemote();
  return settingsSearchHandler;
}
