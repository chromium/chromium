// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

/**
 * An interface for a class that can provide screenshots to a consumer. TODO(crbug.com/40107491):
 * Remove this temporary class and instead move
 * chrome/android/java/src/org/chromium/chrome/browser/feedback/ScreenshotSource.java.
 */
public interface EditorScreenshotSource {
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
