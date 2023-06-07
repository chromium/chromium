// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Uma recorder for secondary activity back press handling.
 */
public class SecondaryActivityBackPressUma {
    @IntDef({SecondaryActivity.DOWNLOAD, SecondaryActivity.BOOKMARK, SecondaryActivity.FIRST_RUN,
            SecondaryActivity.LIGHTWEIGHT_FIRST_RUN, SecondaryActivity.HISTORY,
            SecondaryActivity.SETTINGS, SecondaryActivity.NUM_TYPES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SecondaryActivity {
        int DOWNLOAD = 0;
        int BOOKMARK = 1;
        int FIRST_RUN = 2;
        int LIGHTWEIGHT_FIRST_RUN = 3;
        int HISTORY = 4;
        int SETTINGS = 5;

        int NUM_TYPES = 6;
    }

    public static void record(@SecondaryActivity int activity) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BackPress.SecondaryActivity", activity, SecondaryActivity.NUM_TYPES);
    }
}
