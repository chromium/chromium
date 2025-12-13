// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.back_press.BackPressMetrics.CaptureNativeViewResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.delegate.ScreenshotResult;
import org.chromium.ui.resources.dynamics.CaptureObserver;
import org.chromium.ui.resources.dynamics.CaptureUtils;
import org.chromium.url.GURL;

/** Capture native page as a bitmap. */
@NullMarked
public class NativePageBitmapCapturer {
    // Share SoftwareDraw in order to share a single java Bitmap across all tabs in a window
    // as the tab size won't change inside one single window.
    private static final UnownedUserDataKey<NativePageBitmapCapturer> CAPTURER_KEY =
            new UnownedUserDataKey<>();
    private static final float SCALE = 1;

    private static boolean sIgnoreCurrentUrlCheck;

    private @Nullable HardwareDraw mHardwareDraw;

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
            Tab tab,
            Callback<@Nullable ScreenshotResult> callback,
            ScreenshotResult.Destination destination) {
        if (!isCapturable(tab)) {
            return false;
        }

        int result = shouldUseFallbackUx(tab);
        BackPressMetrics.recordCaptureNativeViewResult(result);
        if (result != CaptureNativeViewResult.CAPTURE_SCREENSHOT) {
            PostTask.postTask(TaskTraits.UI_USER_VISIBLE, () -> callback.onResult(null));
            return true;
        }

        UnownedUserDataHost host = tab.getWindowAndroidChecked().getUnownedUserDataHost();
        if (CAPTURER_KEY.retrieveDataFromHost(host) == null) {
            CAPTURER_KEY.attachToHost(host, new NativePageBitmapCapturer());
        }
        final NativePageBitmapCapturer capturer = CAPTURER_KEY.retrieveDataFromHost(host);
        assumeNonNull(capturer);
        if (enableHardwareDraw()) {
            if (capturer.mHardwareDraw == null) {
                capturer.mHardwareDraw = new HardwareDraw();
            }

            assumeNonNull(tab.getWebContents());
            assumeNonNull(tab.getWebContents().getViewAndroidDelegate());
            assumeNonNull(tab.getWebContents().getViewAndroidDelegate().getContainerView());
            return capturer.mHardwareDraw.startBitmapCapture(
                    assumeNonNull(tab.getView()),
                    tab.getWebContents().getViewAndroidDelegate().getContainerView().getHeight(),
                    SCALE,
                    new CaptureObserver() {
                        @Override
                        public void onCaptureStart(Canvas canvas, @Nullable Rect dirtyRect) {
                            assumeNonNull(tab.getNativePage());
                            canvas.drawColor(tab.getNativePage().getBackgroundColor());
                            canvas.translate(
                                    0, -tab.getNativePage().getHeightOverlappedWithTopControls());
                        }

                        @Override
                        public void onCaptureEnd() {}
                    },
                    callback,
                    destination);
        } else {
            assert destination == ScreenshotResult.Destination.BITMAP;
            Bitmap bitmap = capture(tab, false, 0);
            PostTask.postTask(
                    TaskTraits.UI_USER_VISIBLE,
                    () -> callback.onResult(new ScreenshotResult(bitmap)));
        }
        return true;
    }

    /**
     * Synchronous version of {@link #maybeCaptureNativeView()}.
     *
     * @param tab The target tab to be captured.
     * @param topControlsHeight The height of the top controls.
     * @return Null if fails; otherwise, a Bitmap object.
     */
    public static @Nullable Bitmap maybeCaptureNativeViewSync(Tab tab, int topControlsHeight) {
        if (!isCapturable(tab)) {
            return null;
        }

        return capture(tab, true, topControlsHeight);
    }

    public static void setIgnoreCurrentUrlCheckForTesting() {
        sIgnoreCurrentUrlCheck = true;
        ResettersForTesting.register(
                () -> {
                    sIgnoreCurrentUrlCheck = false;
                });
    }

    private static boolean isCapturable(Tab tab) {
        if (!tab.isNativePage()) {
            return false;
        }
        // The native page, like NTP, is displayed before the url is loaded. Return early to
        // prevent capturing the current NTP as the screenshot of the previous page
        assumeNonNull(tab.getWebContents());
        GURL lastCommittedUrl = tab.getWebContents().getLastCommittedUrl();
        if (!NativePage.isNativePageUrl(lastCommittedUrl, tab.isIncognitoBranded(), false)) {
            return false;
        }

        return true;
    }

    private static @CaptureNativeViewResult int shouldUseFallbackUx(Tab tab) {
        if (tab.getWindowAndroid() == null) {
            return CaptureNativeViewResult.NULL_WINDOW_ANDROID;
        }

        View view = assumeNonNull(tab.getView());
        // The view is not laid out yet.
        if (view.getWidth() == 0 || view.getHeight() == 0) {
            return CaptureNativeViewResult.VIEW_NOT_LAID_OUT;
        }
        if (tab.getWebContents() == null
                || tab.getWebContents().getViewAndroidDelegate() == null
                || tab.getWebContents().getViewAndroidDelegate().getContainerView() == null
                || tab.getWebContents().getViewAndroidDelegate().getContainerView().getHeight()
                        == 0) {
            return CaptureNativeViewResult.VIEW_NOT_LAID_OUT;
        }

        if (!sIgnoreCurrentUrlCheck) {
            GURL lastCommittedUrl = tab.getWebContents().getLastCommittedUrl();
            boolean isLastPageNative =
                    NativePage.isNativePageUrl(lastCommittedUrl, tab.isIncognitoBranded(), false);
            // crbug.com/376115165: Show fallback screenshots when navigating between native pages.
            // Native page views show before the navigation commit, which causes the content/ to
            // capture a wrong screenshot.
            // TODO(crbug.com/378565245): Capture screenshots when navigating between native pages.
            if (isLastPageNative
                    && NativePage.isNativePageUrl(tab.getUrl(), tab.isIncognitoBranded(), false)) {
                return CaptureNativeViewResult.BETWEEN_NATIVE_PAGES;
            }
        }

        return CaptureNativeViewResult.CAPTURE_SCREENSHOT;
    }

    private static Bitmap capture(Tab tab, boolean fullscreen, int topControlsHeight) {
        try (TraceEvent e = TraceEvent.scoped("NativePageBitmapCapturer::capture")) {
            View view = assumeNonNull(tab.getView());
            // The size of the webpage might be different from that of native pages.
            // The former may also capture the area underneath the navigation bar while
            // the latter sometimes does not. If their sizes don't match, a fallback screenshot will
            // be used instead.
            assumeNonNull(tab.getWebContents());
            assumeNonNull(tab.getWebContents().getViewAndroidDelegate());
            assumeNonNull(tab.getWebContents().getViewAndroidDelegate().getContainerView());
            Bitmap bitmap =
                    CaptureUtils.createBitmap(
                            view.getWidth(),
                            tab.getWebContents()
                                    .getViewAndroidDelegate()
                                    .getContainerView()
                                    .getHeight());

            bitmap.eraseColor(assumeNonNull(tab.getNativePage()).getBackgroundColor());

            Canvas canvas = new Canvas(bitmap);

            // Translate to exclude the area of the top controls if present.
            // When requesting a fullscreen bitmap, the content will translate the layer up. Add
            // top controls height to translate the content down to counter that.
            canvas.translate(
                    0,
                    -tab.getNativePage().getHeightOverlappedWithTopControls()
                            + (fullscreen ? topControlsHeight : 0));
            view.draw(canvas);
            return bitmap;
        }
    }

    private static boolean enableHardwareDraw() {
        // LINT.IfChange(minSupportedVersion)
        final var minSupportedVersion = Build.VERSION_CODES.S;
        // LINT.ThenChange(//content/browser/renderer_host/navigation_transitions/navigation_transition_utils.cc:min_supported_version)
        return Build.VERSION.SDK_INT >= minSupportedVersion;
    }
}
