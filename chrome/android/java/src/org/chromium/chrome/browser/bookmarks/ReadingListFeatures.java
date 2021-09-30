// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/**
 * Reading List feature flags and params.
 *
 * <p>{@link ChromeFeatureList#READ_LATER}
 * <ul>
 * <li>{@code session_length}: int (seconds); duration Chrome needs to spend in background before it
 * discards the "last bookmark location". Default: {@link #DEFAULT_SESSION_LENGTH_SECONDS}</li>
 * <li>{@code use_root_bookmark_as_default}: boolean; use the root folder rather than "Mobile
 * bookmarks" as the default bookmark folder. Default: false</li>
 * </ul>
 */
public class ReadingListFeatures {
    private static final int DEFAULT_SESSION_LENGTH_SECONDS = (int) TimeUnit.HOURS.toSeconds(1);

    private ReadingListFeatures() {}

    public static boolean isReadingListEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.READ_LATER);
    }

    /** Returns whether the root folder should be used as the default location. */
    public static boolean shouldUseRootFolderAsDefaultForReadLater() {
        return isReadingListEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.READ_LATER, "use_root_bookmark_as_default", false);
    }

    /**
     * Returns the duration Chrome needs to spend in background before it discards the "last
     * bookmark location".
     */
    public static int getSessionLengthMs() {
        return (int) TimeUnit.SECONDS.toMillis(ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.READ_LATER, "session_length", DEFAULT_SESSION_LENGTH_SECONDS));
    }
}
