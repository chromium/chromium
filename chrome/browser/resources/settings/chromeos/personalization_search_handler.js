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

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import '../search/personalization_search.mojom-lite.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

/** @type {?ash.personalizationApp.mojom.SearchHandlerInterface} */
let personalizationSearchHandler = null;

/**
 * @param {!ash.personalizationApp.mojom.SearchHandlerInterface}
 *     testSearchHandler A test search handler.
 */
export function setPersonalizationSearchHandlerForTesting(testSearchHandler) {
  personalizationSearchHandler = testSearchHandler;
}

/**
 * @return {!ash.personalizationApp.mojom.SearchHandlerInterface} Search
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
    personalizationSearchHandler =
        ash.personalizationApp.mojom.SearchHandler.getRemote();
  }

  return personalizationSearchHandler;
}
