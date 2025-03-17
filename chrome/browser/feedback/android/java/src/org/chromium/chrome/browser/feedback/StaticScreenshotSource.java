// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.graphics.Bitmap;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A implementation of {@link ScreenshotSource} that returns back a {@link Bitmap} given to it. */
@NullMarked
class StaticScreenshotSource implements ScreenshotSource {
    private final @Nullable Bitmap mBitmap;

    /**
     * Creates a new {@link StaticScreenshotSource}.
     * @param screenshot The {@link Bitmap} to use as a screenshot.
     */
    public StaticScreenshotSource(@Nullable Bitmap screenshot) {
        mBitmap = screenshot;
    }

    // ScreenshotSource implementation.
    @Override
    public void capture(Runnable callback) {
        PostTask.postTask(TaskTraits.UI_DEFAULT, callback);
    }

    @Override
    public boolean isReady() {
        return true;
    }

    @Override
    public @Nullable Bitmap getScreenshot() {
        return mBitmap;
    }
}
