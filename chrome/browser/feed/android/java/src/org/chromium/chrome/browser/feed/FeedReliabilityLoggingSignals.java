// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;

/** Interface for logging reliability-related events from outside the feed. */
public interface FeedReliabilityLoggingSignals extends UrlFocusChangeListener {
    /**
     * Perform feed launch logging when the omnibox is focused.
     */
    void onOmniboxFocused();

    /**
     * Perform feed launch logging when the user does a voice search.
     */
    void onVoiceSearch();

    /**
     * This method is repeated from UrlFocusChangeListener to avoid an R8 error related to default
     * interface methods being unsupported before Android N.
     */
    @Override
    void onUrlAnimationFinished(boolean hasFocus);
}