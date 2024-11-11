// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;

import java.io.File;

public class AuxiliarySearchUtils {
    @VisibleForTesting static final String TAB_DONATE_FILE_NAME = "tabs_donate";
    private static final String ZERO_STATE_FAVICON_NUMBER_PARAM = "zero_state_favicon_number";
    public static final IntCachedFieldTrialParameter ZERO_STATE_FAVICON_NUMBER =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    ZERO_STATE_FAVICON_NUMBER_PARAM,
                    AuxiliarySearchProvider.DEFAULT_FAVICON_NUMBER);

    private static final String USE_LARGE_FAVICON_PARAM = "use_large_favicon";
    public static final BooleanCachedFieldTrialParameter USE_LARGE_FAVICON =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    USE_LARGE_FAVICON_PARAM,
                    false);

    private static final String SCHEDULE_DELAY_TIME_MS_PARAM = "schedule_delay_time_ms";
    public static final IntCachedFieldTrialParameter SCHEDULE_DELAY_TIME_MS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    SCHEDULE_DELAY_TIME_MS_PARAM,
                    AuxiliarySearchProvider.DEFAULT_SCHEDULE_DELAY_TIME_MS);

    @VisibleForTesting
    public static int getFaviconSize(Resources resources) {
        return USE_LARGE_FAVICON.getValue()
                ? resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size)
                : resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size_small);
    }

    /** Returns the file to save the metadata for donating tabs. */
    @VisibleForTesting
    public static File getTabDonateFile(Context context) {
        return new File(context.getFilesDir(), TAB_DONATE_FILE_NAME);
    }
}
