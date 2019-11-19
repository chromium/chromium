// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.util.Pair;

import androidx.annotation.Nullable;

import java.util.Map;

/**
 * Used by various components to provide a set of feedback that can be gathered synchronously.
 */
public interface FeedbackSource {
    /**
     * Called to get all relevant feedback for this source.
     * @return A map of all feedback reported by this source.
     */
    // clang-format off
    // TODO(crbug.com/781018): Clang isn't formatting this correctly.
    default @Nullable Map<String, String> getFeedback() {
        return null;
    }
    // clang-format on

    /**
     * Returns a key-value pair of logs for this source.  It is appropriate to
     * return a larger than normal amount of data here and it will be automatically
     * handled by the consumer.  Each source can only return (at most) one
     * key-value log pair.
     * @return A key-value pair representing the logs for this source and the
     *         identifier.
     */
    // clang-format off
    // TODO(crbug.com/781018): Clang isn't formatting this correctly.
    default @Nullable Pair<String, String> getLogs() {
        return null;
    }
        // clang-format on
}