// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.resources.dynamics.CaptureObserver;
import org.chromium.ui.resources.dynamics.SoftwareDraw;

/** Capture native page as a bitmap. */
public class NativePageBitmapCapturer implements UnownedUserData {
    // Share SoftwareDraw in order to share a single java Bitmap across all tabs in a window
    // as the tab size won't change inside one single window.
    private static final UnownedUserDataKey<NativePageBitmapCapturer> CAPTURER_KEY =
            new UnownedUserDataKey<>(NativePageBitmapCapturer.class);
    private final SoftwareDraw mSoftwareDraw = new SoftwareDraw();

    private NativePageBitmapCapturer() {}

    /**
     * Capture native page as a bitmap.
     *
     * @param tab The target tab to be captured.
     * @param callback Executed with a non-null bitmap if the tab is presenting a native page. Empty
     *     bitmap if capturing fails, such as out of memory error.
     * @return True if the capture is successfully triggered; otherwise false.
     */
    public static boolean maybeCaptureNativeView(
            @NonNull Tab tab, @NonNull Callback<Bitmap> callback) {
        if (!tab.isNativePage()) {
            return false;
        }
        if (tab.getWindowAndroid() == null) return false;

        UnownedUserDataHost host = tab.getWindowAndroid().getUnownedUserDataHost();
        if (CAPTURER_KEY.retrieveDataFromHost(host) == null) {
            CAPTURER_KEY.attachToHost(host, new NativePageBitmapCapturer());
        }
        final var capturer = CAPTURER_KEY.retrieveDataFromHost(host);

        View view = tab.getView();
        Rect viewBound = new Rect(0, 0, view.getWidth(), view.getHeight());

        // TODO(crbug.com/330230340): capture bitmap asynchronously.
        capturer.mSoftwareDraw.startBitmapCapture(
                view,
                viewBound,
                capturer.getScale(),
                new CaptureObserver() {
                    @Override
                    public void onCaptureStart(Canvas canvas, Rect dirtyRect) {}

                    @Override
                    public void onCaptureEnd() {}
                },
                callback);
        return true;
    }

    private float getScale() {
        return (float)
                ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS, "downscale", 1);
    }
}
