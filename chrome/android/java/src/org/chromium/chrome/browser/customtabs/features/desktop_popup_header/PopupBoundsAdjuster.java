// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import static android.view.Display.INVALID_DISPLAY;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.graphics.Rect;
import android.os.Build;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowResizePrecheckResult;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver;

/**
 * Adjusts the window bounds of a popup so that its viewport matches the size requested by the
 * opener in its {@code window.open()} call.
 */
@NullMarked
public final class PopupBoundsAdjuster {
    private static final String TAG = "PopupBoundsAdjuster";

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final TabWindowManager mTabWindowManager;

    /**
     * @param windowAndroid The {@link WindowAndroid} of the popup activity.
     * @param tabWindowManager Used to look up the tab that opened the popup.
     */
    public PopupBoundsAdjuster(WindowAndroid windowAndroid, TabWindowManager tabWindowManager) {
        mActivity = assertNonNull(windowAndroid.getActivity().get());
        mWindowAndroid = windowAndroid;
        mTabWindowManager = tabWindowManager;
    }

    /**
     * Adjusts the window bounds of the popup to match the requested {@link WindowFeatures} using at
     * most one {@link android.app.ActivityManager.AppTask#moveTaskTo} call on best-effort basis.
     *
     * @param requestedWindowFeaturesDp The window features requested by the opener.
     * @param popupTab The tab shown in the popup.
     */
    public void adjust(WindowFeatures requestedWindowFeaturesDp, Tab popupTab) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN) {
            return;
        }

        final ChromeAndroidTask task =
                ChromeAndroidTaskTrackerFactory.getInstance().get(mActivity.getTaskId());
        if (task == null || task.canResize() != WindowResizePrecheckResult.OK) {
            Log.v(TAG, "adjustWindowBounds: the task cannot be resized -- bailing out");
            return;
        }

        final WebContents webContents = popupTab.getWebContents();
        if (webContents == null) {
            Log.w(TAG, "adjustWindowBounds: webContents is null -- bailing out");
            return;
        }

        final int realViewportWidthDp = webContents.getWidth();
        final int realViewportHeightDp = webContents.getHeight();
        final Rect realWindowBoundsPx =
                new Rect(mActivity.getWindowManager().getCurrentWindowMetrics().getBounds());
        final Rect targetWindowBoundsPx = new Rect(realWindowBoundsPx);
        Log.v(
                TAG,
                "adjustWindowBounds: current viewport size = (%d x %d) [dp]",
                realViewportWidthDp,
                realViewportHeightDp);
        Log.v(TAG, "adjustWindowBounds: top-level window bounds = %s [px]", realWindowBoundsPx);
        Log.v(
                TAG,
                "adjustWindowBounds: requested WindowFeatures = %s [dp]",
                requestedWindowFeaturesDp);

        if (requestedWindowFeaturesDp.width != null && requestedWindowFeaturesDp.height != null) {
            DisplayAndroid display = mWindowAndroid.getDisplay();

            final int widthDiffDp = requestedWindowFeaturesDp.width - realViewportWidthDp;
            final int heightDiffDp = requestedWindowFeaturesDp.height - realViewportHeightDp;

            final int widthDiffPx = DisplayUtil.dpToPx(display, widthDiffDp);
            final int heightDiffPx = DisplayUtil.dpToPx(display, heightDiffDp);

            targetWindowBoundsPx.right += widthDiffPx;
            targetWindowBoundsPx.bottom += heightDiffPx;

            final boolean isPopupOpenedCrossDisplay =
                    getDisplayIdFromTabId(popupTab.getParentId()) != display.getDisplayId();
            final String histogramVariant =
                    isPopupOpenedCrossDisplay ? "CrossDisplay" : "InDisplay";

            // If the display's dipScale is less than 1 it may happen that the difference in dps is
            // non-zero while the same difference in px is zero. In such case we consider it a
            // success as we could not have done anything better than being pixel perfect.

            RecordHistogram.recordBooleanHistogram(
                    "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaWidth.Outcome."
                            + histogramVariant,
                    widthDiffPx != 0);
            if (widthDiffPx != 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaWidth.Positive."
                                + histogramVariant,
                        Math.abs(widthDiffDp));
            }

            RecordHistogram.recordBooleanHistogram(
                    "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaHeight.Outcome."
                            + histogramVariant,
                    heightDiffPx != 0);
            if (heightDiffPx != 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaHeight.Positive."
                                + histogramVariant,
                        Math.abs(heightDiffDp));
            }

            Log.v(TAG, "adjustWindowBounds: widthDiffDp = %d dp", widthDiffDp);
            Log.v(TAG, "adjustWindowBounds: widthDiffPx = %d px", widthDiffPx);
            Log.v(TAG, "adjustWindowBounds: heightDiffDp = %d dp", heightDiffDp);
            Log.v(TAG, "adjustWindowBounds: heightDiffPx = %d px", heightDiffPx);
            Log.v(TAG, "adjustWindowBounds: display.getDipScale() = %s", display.getDipScale());

            final InsetObserver insetObserver = mWindowAndroid.getInsetObserver();
            Insets windowInsetsOnSourceDisplay = Insets.NONE;
            if (insetObserver != null && insetObserver.getLastRawWindowInsets() != null) {
                windowInsetsOnSourceDisplay =
                        insetObserver
                                .getLastRawWindowInsets()
                                .getInsets(WindowInsetsCompat.Type.captionBar());
            }
            Log.v(
                    TAG,
                    "adjustWindowBounds: current window insets = %s [px]",
                    windowInsetsOnSourceDisplay);
        }

        if (realWindowBoundsPx.equals(targetWindowBoundsPx)) {
            Log.v(TAG, "adjustWindowBounds: perfect match -- no repositioning required");
            return;
        }
        Log.v(
                TAG,
                "adjustWindowBounds: repositioning required -- current bounds = %s, target bounds ="
                        + " %s",
                realWindowBoundsPx,
                targetWindowBoundsPx);

        final AppTask appTask = AndroidTaskUtils.getAppTaskFromId(mActivity, mActivity.getTaskId());
        if (appTask == null) {
            Log.w(TAG, "adjustWindowBounds: cannot find the AppTask -- bailing out");
            return;
        }

        Log.v(TAG, "adjustWindowBounds: dispatching the moveTaskTo call");
        AndroidTaskUtils.moveTaskTo(
                appTask, INVALID_DISPLAY /* current display */, targetWindowBoundsPx);
    }

    private int getDisplayIdFromTabId(int tabId) {
        final Tab tab = mTabWindowManager.getTabById(tabId);
        if (tab == null) {
            return INVALID_DISPLAY;
        }
        final WindowAndroid window = tab.getWindowAndroid();
        if (window == null) {
            return INVALID_DISPLAY;
        }
        return window.getDisplay().getDisplayId();
    }
}
