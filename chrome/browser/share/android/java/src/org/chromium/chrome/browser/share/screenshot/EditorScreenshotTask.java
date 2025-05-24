// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * A utility class to take a screenshot of an {@link Activity}. TODO(crbug.com/40107491): Remove
 * this temporary class and instead move
 * chrome/android/java/src/org/chromium/chrome/browser/feedback/ScreenshotTask.java.
 */
@JNINamespace("android")
public final class EditorScreenshotTask implements EditorScreenshotSource {
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;

    private boolean mDone;
    private Bitmap mBitmap;
    private Runnable mCallback;

    /**
     * Creates a {@link EditorScreenshotTask} instance that, will grab a screenshot of {@code
     * activity}.
     * @param activity The {@link Activity} to grab a screenshot of.
     * @param sheetController A {@link BottomSheetController} to check if the sheet is showing.
     */
    public EditorScreenshotTask(Activity activity, BottomSheetController sheetController) {
        mActivity = activity;
        mBottomSheetController = sheetController;
    }

    // ScreenshotSource implementation.
    @Override
    public void capture(@Nullable Runnable callback) {
        mCallback = callback;

        if (takeCompositorScreenshot(mActivity)) return;
        if (takeAndroidViewScreenshot(mActivity)) return;

        // If neither the compositor nor the Android view screenshot tasks were kicked off, admit
        // defeat and return a {@code null} screenshot.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
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
    // EditorScreenshotTaskJni.get().grabWindowSnapshotAsync.
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
        if (activity == null || !shouldTakeCompositorScreenshot(activity)) return false;

        Rect rect = new Rect();
        activity.getWindow().getDecorView().getRootView().getWindowVisibleDisplayFrame(rect);
        EditorScreenshotTaskJni.get()
                .grabWindowSnapshotAsync(
                        this,
                        ((ChromeActivity) activity).getWindowAndroid(),
                        rect.width(),
                        rect.height());

        return true;
    }

    private boolean takeAndroidViewScreenshot(@Nullable final Activity activity) {
        if (activity == null) return false;

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        Bitmap bitmap =
                                UiUtils.generateScaledScreenshot(
                                        activity.getWindow().getDecorView().getRootView(),
                                        0,
                                        Bitmap.Config.ARGB_8888);
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
        // TODO(crbug.com/40573072): When the sheet is partially opened both the compositor
        // and Android views should be captured in the screenshot.
        if (mBottomSheetController.isSheetOpen()) return false;

        // If the tab is null, assume in the tab switcher so a Compositor snapshot is good.
        if (currentTab == null) return true;
        // If the tab is not interactable, also assume in the tab switcher.
        if (!currentTab.isUserInteractable()) return true;
        // If the tab focused and not showing Android widget based content, then use the Compositor
        // based screenshot.
        if (currentTab.getNativePage() == null && !currentTab.isShowingCustomView()) return true;

        // Assume the UI is drawn primarily by Android widgets, so do not use the Compositor
        // screenshot.
        return false;
    }

    @NativeMethods
    interface Natives {
        void grabWindowSnapshotAsync(
                EditorScreenshotTask callback, WindowAndroid window, int width, int height);
    }
}
