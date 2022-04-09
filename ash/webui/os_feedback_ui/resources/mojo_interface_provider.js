// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {fakeFeedbackContext} from './fake_data.js';
import {FakeFeedbackServiceProvider} from './fake_feedback_service_provider.js';
import {FeedbackServiceProviderInterface, HelpContentProvider, HelpContentProviderInterface} from './feedback_types.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * @type {?FeedbackServiceProviderInterface}
 */
let feedbackServiceProvider = null;

/**
 * @type {?HelpContentProviderInterface}
 */
let helpContentProvider = null;

/**
 * @param {?FeedbackServiceProviderInterface} testProvider
 */
export function setFeedbackServiceProviderForTesting(testProvider) {
  feedbackServiceProvider = testProvider;
}

/**
 * @param {?HelpContentProviderInterface} testProvider
 */
export function setHelpContentProviderForTesting(testProvider) {
  helpContentProvider = testProvider;
}

/**
 * @return {!FeedbackServiceProviderInterface}
 */
export function getFeedbackServiceProvider() {
  if (!feedbackServiceProvider) {
    // TODO(xiangdongkong): Instantiate a real mojo interface here.
    const fakeProvider = /** @type {FeedbackServiceProviderInterface} */ (
        new FakeFeedbackServiceProvider());
    fakeProvider.setFakeFeedbackContext(fakeFeedbackContext);
    setFeedbackServiceProviderForTesting(fakeProvider);
  }
  assert(!!feedbackServiceProvider);
  return feedbackServiceProvider;
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
