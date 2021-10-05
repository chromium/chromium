// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Self-documenting feature class for bookmarks. */
public class BookmarkFeatures {
    private static final boolean IMPROVED_SAVE_FLOW_AUTODISMISS_ENABLED_DEFAULT = true;
    // This is the same as the default dismiss time for snackbars.
    private static final int IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT = 3000;

    static final String AUTODISMISS_ENABLED_PARAM_NAME = "autodismiss_enabled";
    static final String AUTODISMISS_LENGTH_PARAM_NAME = "autodismiss_length_ms";

    public static boolean isImprovedSaveFlowEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW);
    }

    public static boolean isImprovedSaveFlowAutodismissEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW,
                        AUTODISMISS_ENABLED_PARAM_NAME,
                        IMPROVED_SAVE_FLOW_AUTODISMISS_ENABLED_DEFAULT);
    }

    public static int getImprovedSaveFlowAutodismissTimeMs() {
        if (!FeatureList.isInitialized()) return IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW, AUTODISMISS_LENGTH_PARAM_NAME,
                IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT);
    }
}