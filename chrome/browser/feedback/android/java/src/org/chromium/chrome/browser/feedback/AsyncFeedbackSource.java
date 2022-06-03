// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

/**
 * Used by various components to provide a set of feedback data that can be gathered asynchronously.
 * Note that if the owner times out, {@link #getFeedback()} might be called even if
 * this source isn't ready.  At that point this source should do it's best to provide what data it
 * can.
 */
public interface AsyncFeedbackSource extends FeedbackSource {
    /**
     * Starts the feedback collection process for this source.  This source should notify
     * {@code callback} when the collection is finished.
     * @param callback The callback to notify when the collection is finished.
     */
    void start(Runnable callback);

    /**
     * @return Whether or not this source is ready to provide feedback.
     */
    boolean isReady();
}