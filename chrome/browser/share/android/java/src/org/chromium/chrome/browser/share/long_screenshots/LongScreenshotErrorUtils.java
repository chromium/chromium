// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.content.Context;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.widget.Toast;

/** Utility class to show a consistent error UI for long screenshots across different flows. */
@NullMarked
public class LongScreenshotErrorUtils {
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
