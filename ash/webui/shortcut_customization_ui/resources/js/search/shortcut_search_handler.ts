// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {fakeSearchResults} from '../fake_data.js';
import {ShortcutSearchHandler, ShortcutSearchHandlerInterface} from '../shortcut_types.js';
import {isSearchEnabled} from '../shortcut_utils.js';

import {FakeShortcutSearchHandler} from './fake_shortcut_search_handler.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */
let shortcutSearchHandler: ShortcutSearchHandlerInterface|null = null;

export function setShortcutSearchHandlerForTesting(
    testHandler: ShortcutSearchHandlerInterface): void {
  shortcutSearchHandler = testHandler;
}

/**
 * Create a Fake ShortcutSearchHandler with reasonable fake data.
 */
export function setupFakeShortcutSearchHandler(): void {
  // Create handler.
  const handler = new FakeShortcutSearchHandler();

  // Setup search response.
  handler.setFakeSearchResult(fakeSearchResults);

  // Set the fake handler.
  setShortcutSearchHandlerForTesting(handler);
}

export function getShortcutSearchHandler(): ShortcutSearchHandlerInterface {
  if (!shortcutSearchHandler) {
    if (isSearchEnabled()) {
      shortcutSearchHandler = ShortcutSearchHandler.getRemote();
    } else {
      setupFakeShortcutSearchHandler();
    }
  }
  assert(!!shortcutSearchHandler);
  return shortcutSearchHandler;
}
