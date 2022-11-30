// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

/**
 * FeedbackReporter enables Chrome to send feedback to the feedback server.
 */
public interface FeedbackReporter {
    /**
     * Report feedback to the feedback server.
     *
     * @param collector the {@link FeedbackCollector} to use for extra data.
     */
    default void reportFeedback(FeedbackCollector collector) {}
}
