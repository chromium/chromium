// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

/**
 * Listener for new {@link ContinuousNavigationMetadata}.
 */
public interface SearchResultListener {
    /**
     * Called when returning set of results.
     * @param metadata The result data.
     */
    void onResult(ContinuousNavigationMetadata metadata);

    /**
     * Called when there is an error getting results.
     * @param errorCode A code signifying what error occurred.
     */
    void onError(int errorCode);
}
