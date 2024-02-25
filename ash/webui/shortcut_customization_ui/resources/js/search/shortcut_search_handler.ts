// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {ShortcutSearchHandler, ShortcutSearchHandlerInterface} from '../shortcut_types.js';

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

export function getShortcutSearchHandler(): ShortcutSearchHandlerInterface {
  if (!shortcutSearchHandler) {
    shortcutSearchHandler = ShortcutSearchHandler.getRemote();
  }
  assert(!!shortcutSearchHandler);
  return shortcutSearchHandler;
}
