// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Utility class for long screenshots across different flows. */
@NullMarked
public class LongScreenshotsUtils {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        BitmapGeneratorStatus.CAPTURE_COMPLETE,
        BitmapGeneratorStatus.INSUFFICIENT_MEMORY,
        BitmapGeneratorStatus.GENERATION_ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface BitmapGeneratorStatus {
        int CAPTURE_COMPLETE = 0;
        int INSUFFICIENT_MEMORY = 1;
        int GENERATION_ERROR = 2;
        int COUNT = 3;
    }

    /**
     * Shows a generic error Toast for long screenshot failures.
     *
     * @param context The context to use for the Toast.
     */
    public static void showErrorMessage(Context context) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Toast.makeText(
                                    context,
                                    R.string.sharing_long_screenshot_unknown_error,
                                    Toast.LENGTH_LONG)
                            .show();
                });
    }
}
