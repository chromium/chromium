// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {FeedbackServiceProvider, FeedbackServiceProviderInterface, HelpContentProvider, HelpContentProviderInterface} from './os_feedback_ui.mojom-webui.js';

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
    feedbackServiceProvider = FeedbackServiceProvider.getRemote();
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
