// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import android.app.Activity;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.util.Size;
import android.view.View;
import android.view.Window;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.actor.OffscreenRenderingManager;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxTabUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.utilities.OnDemandBackgroundTabCaptureConfig;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

/**
 * Manages the lifecycle and offscreen window dimensions for background tabs undergoing on-demand
 * capture in {@link TabItemPickerCoordinator}.
 */
@NullMarked
public class TabItemPickerOffscreenRenderer {
    private final Activity mActivity;
    private final Set<Tab> mOffscreenRenderingTabs = new HashSet<>();

    /**
     * Constructs a {@link TabItemPickerOffscreenRenderer}.
     *
     * @param activity The host {@link Activity} used to calculate viewport rendering dimensions.
     */
    public TabItemPickerOffscreenRenderer(Activity activity) {
        mActivity = activity;
    }

    /**
     * Initiates offscreen rendering for a background tab if optimization is enabled.
     *
     * <p>Attaches the tab's {@link WebContents} to an offscreen window and compositor via {@link
     * OffscreenRenderingManager}, allowing the headless web page to produce root layer frame
     * updates necessary for visually non-empty paint notifications. Active foreground tabs are
     * skipped since their visible renderers already produce frame updates.
     *
     * @param tab The {@link Tab} to start rendering offscreen.
     */
    public void startOffscreenRenderingIfNeeded(Tab tab) {
        if (!OnDemandBackgroundTabCaptureConfig.isOffscreenRenderingEnabled()
                || mActivity.isFinishing()
                || mActivity.isDestroyed()
                || FuseboxTabUtils.isTabActive(tab)) {
            return;
        }
        if (!mOffscreenRenderingTabs.add(tab)) {
            return;
        }

        Size size = getOffscreenRenderingSize(mActivity);
        OffscreenRenderingManager.getInstance()
                .startOffscreenRendering(tab, size.getWidth(), size.getHeight());
    }

    /**
     * Stops offscreen rendering for a tab and detaches its native compositor structures.
     *
     * <p>Guards against already destroyed WebContents to prevent native crashes during teardown.
     *
     * @param tab The {@link Tab} to stop rendering offscreen.
     */
    public void stopOffscreenRenderingIfNeeded(Tab tab) {
        if (OnDemandBackgroundTabCaptureConfig.isOffscreenRenderingEnabled()
                && mOffscreenRenderingTabs.remove(tab)) {
            WebContents webContents = tab.getWebContents();
            if (webContents != null && !webContents.isDestroyed()) {
                OffscreenRenderingManager.getInstance().stopOffscreenRendering(tab);
            }
        }
    }

    /** Cleans up and detaches all active offscreen rendering sessions. */
    public void destroy() {
        for (Tab tab : new ArrayList<>(mOffscreenRenderingTabs)) {
            stopOffscreenRenderingIfNeeded(tab);
        }
        mOffscreenRenderingTabs.clear();
    }

    /** Returns the target width and height for offscreen rendering. */
    private static Size getOffscreenRenderingSize(Activity activity) {
        Window window = activity.getWindow();
        View decorView = window != null ? window.peekDecorView() : null;
        int width = decorView != null ? decorView.getWidth() : 0;
        int height = decorView != null ? decorView.getHeight() : 0;

        if (width <= 0 || height <= 0) {
            Resources resources = activity.getResources();
            DisplayMetrics displayMetrics =
                    resources != null ? resources.getDisplayMetrics() : null;
            width = displayMetrics != null ? displayMetrics.widthPixels : 1;
            height = displayMetrics != null ? displayMetrics.heightPixels : 1;
        }

        return new Size(Math.max(1, width), Math.max(1, height));
    }

    @VisibleForTesting
    public Set<Tab> getOffscreenRenderingTabsForTesting() {
        return mOffscreenRenderingTabs;
    }
}
