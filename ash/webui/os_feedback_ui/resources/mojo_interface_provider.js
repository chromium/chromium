// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {fakeHelpContentList, fakeSearchResponse} from './fake_data.js';
import {FakeHelpContentProvider} from './fake_help_content_provider.js';
import {HelpContentProviderInterface} from './feedback_types.js';

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
 * Create a FakeHelpContentProvider with reasonable fake data.
 * TODO(xiangdongkong): Remove once mojo bindings are implemented.
 */
function setupFakeHelpContentProvider() {
  // Create provider.
  const provider = new FakeHelpContentProvider();

  // Setup search response.
  provider.setFakeSearchResponse(fakeSearchResponse);

  // Set the fake provider.
  setHelpContentProviderForTesting(provider);
}

/**
 * @return {!HelpContentProviderInterface}
 */
export function getHelpContentProvider() {
  if (!helpContentProvider) {
    // TODO(xiangdongkong): Instantiate a real mojo interface here.
    setupFakeHelpContentProvider();
  }

  assert(!!helpContentProvider);
  return helpContentProvider;
}
