// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * A utility class to take a feedback-formatted screenshot of an {@link Activity}.
 */
@JNINamespace("chrome::android")
final class ScreenshotTask implements ScreenshotSource {
    /**
     * Maximum dimension for the screenshot to be sent to the feedback handler.  This size
     * ensures the size of bitmap < 1MB, which is a requirement of the handler.
     */
    private static final int MAX_FEEDBACK_SCREENSHOT_DIMENSION = 600;

    private final Activity mActivity;

    private boolean mDone;
    private Bitmap mBitmap;
    private Runnable mCallback;

    /**
     * Creates a {@link ScreenshotTask} instance that, will grab a screenshot of {@code activity}.
     * @param activity The {@link Activity} to grab a screenshot of.
     */
    public ScreenshotTask(Activity activity) {
        mActivity = activity;
    }

    // ScreenshotSource implementation.
    @Override
    public void capture(@Nullable Runnable callback) {
        mCallback = callback;

        if (takeCompositorScreenshot(mActivity)) return;
        if (takeAndroidViewScreenshot(mActivity)) return;

        // If neither the compositor nor the Android view screenshot tasks were kicked off, admit
        // defeat and return a {@code null} screenshot.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                onBitmapReceived(null);
            }
        });
    }

    @Override
    public boolean isReady() {
        return mDone;
    }

    @Override
    public Bitmap getScreenshot() {
        return mBitmap;
    }

    // This will be called on the UI thread in response to
    // ScreenshotTaskJni.get().grabWindowSnapshotAsync.
    @CalledByNative
    private void onBytesReceived(byte[] pngBytes) {
        Bitmap bitmap = null;
        if (pngBytes != null) bitmap = BitmapFactory.decodeByteArray(pngBytes, 0, pngBytes.length);
        onBitmapReceived(bitmap);
    }

    private void onBitmapReceived(@Nullable Bitmap bitmap) {
        mDone = true;
        mBitmap = bitmap;
        if (mCallback != null) mCallback.run();
        mCallback = null;
    }

    private boolean takeCompositorScreenshot(@Nullable Activity activity) {
        if (!shouldTakeCompositorScreenshot((activity))) return false;

        Rect rect = new Rect();
        activity.getWindow().getDecorView().getRootView().getWindowVisibleDisplayFrame(rect);
        ScreenshotTaskJni.get().grabWindowSnapshotAsync(
                this, ((ChromeActivity) activity).getWindowAndroid(), rect.width(), rect.height());

        return true;
    }

    private boolean takeAndroidViewScreenshot(@Nullable final Activity activity) {
        if (activity == null) return false;

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                Bitmap bitmap = UiUtils.generateScaledScreenshot(
                        activity.getWindow().getDecorView().getRootView(),
                        MAX_FEEDBACK_SCREENSHOT_DIMENSION, Bitmap.Config.ARGB_8888);
                onBitmapReceived(bitmap);
            }
        });

        return true;
    }

    private boolean shouldTakeCompositorScreenshot(Activity activity) {
        // If Activity isn't a ChromeActivity, we aren't using the Compositor to render.
        if (!(activity instanceof ChromeActivity)) return false;

        ChromeActivity chromeActivity = (ChromeActivity) activity;
        Tab currentTab = chromeActivity.getActivityTab();

        // If the bottom sheet is currently open, then do not use the Compositor based screenshot
        // so that the Android View for the bottom sheet will be captured.
        // TODO(https://crbug.com/835862): When the sheet is partially opened both the compositor
        // and Android views should be captured in the screenshot.
        if (chromeActivity.getBottomSheetController().isSheetOpen()) return false;

        // If the tab is null, assume in the tab switcher so a Compositor snapshot is good.
        if (currentTab == null) return true;
        // If the tab is not interactable, also assume in the tab switcher.
        if (!currentTab.isUserInteractable()) return true;
        // If the tab focused and not showing Android widget based content, then use the Compositor
        // based screenshot.
        if (currentTab.getNativePage() == null && !SadTab.isShowing(currentTab)) return true;

        // Assume the UI is drawn primarily by Android widgets, so do not use the Compositor
        // screenshot.
        return false;
    }

    @NativeMethods
    interface Natives {
        void grabWindowSnapshotAsync(
                ScreenshotTask callback, WindowAndroid window, int width, int height);
    }
}
