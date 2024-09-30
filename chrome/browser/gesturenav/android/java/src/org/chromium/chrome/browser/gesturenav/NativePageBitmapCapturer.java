// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.resources.dynamics.CaptureUtils;
import org.chromium.url.GURL;

/** Capture native page as a bitmap. */
public class NativePageBitmapCapturer implements UnownedUserData {
    // Share SoftwareDraw in order to share a single java Bitmap across all tabs in a window
    // as the tab size won't change inside one single window.
    private static final UnownedUserDataKey<NativePageBitmapCapturer> CAPTURER_KEY =
            new UnownedUserDataKey<>(NativePageBitmapCapturer.class);

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
        if (!isCapturable(tab)) {
            return false;
        }

        UnownedUserDataHost host = tab.getWindowAndroid().getUnownedUserDataHost();
        if (CAPTURER_KEY.retrieveDataFromHost(host) == null) {
            CAPTURER_KEY.attachToHost(host, new NativePageBitmapCapturer());
        }

        // TODO(crbug.com/330230340): capture bitmap asynchronously.
        Bitmap bitmap = capture(tab);
        PostTask.postTask(TaskTraits.UI_USER_VISIBLE, () -> callback.onResult(bitmap));
        return true;
    }

    /**
     * Synchronous version of {@link #maybeCaptureNativeView(Tab, Callback)}.
     *
     * @param tab The target tab to be captured.
     * @return Null if fails; otherwise, a Bitmap object.
     */
    @Nullable
    public static Bitmap maybeCaptureNativeViewSync(@NonNull Tab tab) {
        if (!isCapturable(tab)) {
            return null;
        }

        return capture(tab);
    }

    private static boolean isCapturable(Tab tab) {
        if (!tab.isNativePage()) {
            return false;
        }
        // The native page, like NTP, is displayed before the url is loaded. Return early to
        // prevent capturing the current NTP as the screenshot of the previous page
        GURL lastCommittedUrl = tab.getWebContents().getLastCommittedUrl();
        if (!NativePage.isNativePageUrl(lastCommittedUrl, tab.isIncognitoBranded(), false)) {
            return false;
        }
        if (tab.getWindowAndroid() == null) return false;

        View view = tab.getView();
        // The view is not laid out yet.
        if (view.getWidth() == 0 || view.getHeight() == 0) return false;
        if (tab.getWebContents() == null
                || tab.getWebContents().getViewAndroidDelegate() == null
                || tab.getWebContents().getViewAndroidDelegate().getContainerView() == null
                || tab.getWebContents().getViewAndroidDelegate().getContainerView().getHeight()
                        == 0) {
            return false;
        }
        return true;
    }

    private static Bitmap capture(Tab tab) {
        UnownedUserDataHost host = tab.getWindowAndroid().getUnownedUserDataHost();
        if (CAPTURER_KEY.retrieveDataFromHost(host) == null) {
            CAPTURER_KEY.attachToHost(host, new NativePageBitmapCapturer());
        }
        final var capturer = CAPTURER_KEY.retrieveDataFromHost(host);

        View view = tab.getView();
        // The size of the webpage might be different from that of native pages.
        // The former may also capture the area underneath the navigation bar while
        // the latter sometimes does not. If their sizes don't match, a fallback screenshot will
        // be used instead.
        Bitmap bitmap =
                CaptureUtils.createBitmap(
                        view.getWidth(),
                        tab.getWebContents()
                                .getViewAndroidDelegate()
                                .getContainerView()
                                .getHeight());

        bitmap.eraseColor(tab.getNativePage().getBackgroundColor());

        Canvas canvas = new Canvas(bitmap);
        float scale = capturer.getScale();

        // TODO(crbug.com/330230340): capture bitmap asynchronously.

        // Translate to exclude the area of the top controls if present.
        canvas.translate(0, -tab.getNativePage().getHeightOverlappedWithTopControls());
        canvas.scale(scale, scale);
        view.draw(canvas);
        return bitmap;
    }

    private float getScale() {
        return (float)
                ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS, "downscale", 1);
    }
}
