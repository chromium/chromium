// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

/** An interface for a class that can provide screenshots to a consumer. */
public interface ScreenshotSource {
    /**
     * Starts capturing the screenshot.
     * @param callback The {@link Runnable} to call when the screenshot capture process is complete.
     */
    void capture(@Nullable Runnable callback);

    /** @return Whether or not this source is finished attempting to grab a screenshot. */
    boolean isReady();

    /** @return A screenshot if available or {@code null} otherwise. */
    @Nullable
    Bitmap getScreenshot();
}