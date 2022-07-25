// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchHandler, SearchHandlerInterface} from '../mojom-webui/search/search.mojom-webui.js';

/**
 * @fileoverview
 * Provides functions used for OS settings search.
 * Also provides a way to inject a test implementation for verifying
 * OS settings search.
 */
/** @type {?SearchHandlerInterface} */
let settingsSearchHandler = null;

/**
 * @param {!SearchHandlerInterface}
 *     testSearchHandler A test search handler.
 */
export function setSettingsSearchHandlerForTesting(testSearchHandler) {
  settingsSearchHandler = testSearchHandler;
}

/**
 * @return {!SearchHandlerInterface} Search handler.
 */
export function getSettingsSearchHandler() {
  if (settingsSearchHandler) {
    return settingsSearchHandler;
  }

  settingsSearchHandler = SearchHandler.getRemote();
  return settingsSearchHandler;
}
