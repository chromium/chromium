// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {FeedbackServiceProvider, FeedbackServiceProviderInterface, HelpContentProvider, HelpContentProviderInterface} from './os_feedback_ui.mojom-webui.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

let feedbackServiceProvider: FeedbackServiceProviderInterface|null = null;

let helpContentProvider: HelpContentProviderInterface|null = null;

export function setFeedbackServiceProviderForTesting(
    testProvider: FeedbackServiceProviderInterface|null) {
  feedbackServiceProvider = testProvider;
}

export function setHelpContentProviderForTesting(
    testProvider: HelpContentProviderInterface|null) {
  helpContentProvider = testProvider;
}

export function getFeedbackServiceProvider(): FeedbackServiceProviderInterface {
  if (!feedbackServiceProvider) {
    feedbackServiceProvider = FeedbackServiceProvider.getRemote();
  }
  assert(feedbackServiceProvider);
  return feedbackServiceProvider;
}

export function getHelpContentProvider(): HelpContentProviderInterface {
  if (!helpContentProvider) {
    helpContentProvider = HelpContentProvider.getRemote();
  }

  assert(helpContentProvider);
  return helpContentProvider;
}
