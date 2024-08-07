// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.util.Pair;

import androidx.annotation.Nullable;

import java.util.Map;

/**
 * Used by various components to provide a set of feedback that can be gathered synchronously.
 *
 * <p>ATTENTION: Before making any changes or adding new FeedbackSources to feedback collection,
 * please ensure the teams that operationalize feedback are aware and supportive. Contact:
 * chrome-gtech@.
 */
public interface FeedbackSource {
    /**
     * Called to get all relevant feedback for this source.
     *
     * @return A map of all feedback reported by this source.
     */
    default @Nullable Map<String, String> getFeedback() {
        return null;
    }

    /**
     * Returns a key-value pair of logs for this source.  It is appropriate to
     * return a larger than normal amount of data here and it will be automatically
     * handled by the consumer.  Each source can only return (at most) one
     * key-value log pair.
     * @return A key-value pair representing the logs for this source and the
     *         identifier.
     */
    default @Nullable Pair<String, String> getLogs() {
        return null;
    }
}
