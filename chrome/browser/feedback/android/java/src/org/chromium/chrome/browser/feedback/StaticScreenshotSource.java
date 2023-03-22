// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.graphics.Bitmap;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/** A implementation of {@link ScreenshotSource} that returns back a {@link Bitmap} given to it. */
class StaticScreenshotSource implements ScreenshotSource {
    private final Bitmap mBitmap;

    /**
     * Creates a new {@link StaticScreenshotSource}.
     * @param screenshot The {@link Bitmap} to use as a screenshot.
     */
    public StaticScreenshotSource(Bitmap screenshot) {
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
    public Bitmap getScreenshot() {
        return mBitmap;
    }
}
