// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {HelpContentProvider, HelpContentProviderInterface} from './feedback_types.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * @type {?HelpContentProviderInterface}
 */
let helpContentProvider = null;

/**
 * @param {?HelpContentProviderInterface} testProvider
 */
export function setHelpContentProviderForTesting(testProvider) {
  helpContentProvider = testProvider;
}

/**
 * @return {!HelpContentProviderInterface}
 */
export function getHelpContentProvider() {
  if (!helpContentProvider) {
    helpContentProvider = HelpContentProvider.getRemote();
  }

  assert(!!helpContentProvider);
  return helpContentProvider;
}
