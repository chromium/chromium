// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides functions used for personalization search, results of which link to
 * Personalization App.
 * Also provides a way to inject a test implementation for verifying
 * personalization search.
 */

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {SearchHandler, SearchHandlerInterface} from '../mojom-webui/personalization/search.mojom-webui.js';

/** @type {?SearchHandlerInterface} */
let personalizationSearchHandler = null;

/**
 * @param {!SearchHandlerInterface} testSearchHandler A test search handler.
 */
export function setPersonalizationSearchHandlerForTesting(testSearchHandler) {
  personalizationSearchHandler = testSearchHandler;
}

/**
 * @return {!SearchHandlerInterface} Search
 *     handler.
 */
export function getPersonalizationSearchHandler() {
  assert(
      loadTimeData.getBoolean('isPersonalizationHubEnabled'),
      'personalization hub feature is required');
  assert(
      !loadTimeData.getBoolean('isGuest'),
      'guest must not request personalization search handler');
  if (!personalizationSearchHandler) {
    personalizationSearchHandler = SearchHandler.getRemote();
  }

  return personalizationSearchHandler;
}
