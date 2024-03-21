// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_windowing;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetsRectProvider;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

import java.util.Collections;

/**
 * Class coordinating the business logic to draw into app header in desktop windowing mode, ranging
 * from listening the window insets updates, and pushing updates to the tab strip.
 */
@RequiresApi(api = Build.VERSION_CODES.R)
public class AppHeaderCoordinator {
    private static final String TAG = "AppHeader";
    // TODO(crbug/328446763): Use values from Android V and remove SuppressWarnings.
    private static final int APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND = 1 << 7;
    private static final int APPEARANCE_LIGHT_CAPTION_BARS = 1 << 8;

    private static @Nullable InsetsRectProvider sInsetsRectProviderForTesting;

    private final Activity mActivity;
    private final View mCoordinatorView;
    private final InsetsRectProvider mInsetsRectProvider;
    private final StripLayoutHelperManager mStripLayoutHelperManager;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;

    private final WindowInsetsController mInsetsController;
    private final InsetsRectProvider.Observer mInsetsRectUpdateRunnable;

    private boolean mDesktopWindowingEnabled;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;

    private Rect mWidestUnoccludedRect;

    /**
     * Instantiate the coordinator to handle drawing the tab strip into the captionBar area.
     *
     * @param activity The activity associated with the window containing the app header.
     * @param coordinatorView The root view within the activity.
     * @param stripLayoutHelperManager StripLayoutHelperManager that manages the tab strip.
     * @param browserControlsVisibilityDelegate Delegate interface allowing control of the browser
     *     controls visibility.
     * @param insetObserver {@link InsetObserver} that manages insets changes on the
     *     CoordinatorView.
     */
    @SuppressWarnings("WrongConstant")
    public AppHeaderCoordinator(
            Activity activity,
            View coordinatorView,
            StripLayoutHelperManager stripLayoutHelperManager,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            InsetObserver insetObserver) {
        // TODO(crbug/328446763): Properly release the reference to the activity when destroyed.
        mActivity = activity;
        mCoordinatorView = coordinatorView;
        mStripLayoutHelperManager = stripLayoutHelperManager;
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
        mInsetsRectUpdateRunnable = this::onInsetsRectsUpdated;

        mInsetsController = mActivity.getWindow().getDecorView().getWindowInsetsController();

        WindowInsets insets = mActivity.getWindow().getDecorView().getRootWindowInsets();
        mInsetsRectProvider =
                sInsetsRectProviderForTesting != null
                        ? sInsetsRectProviderForTesting
                        : new InsetsRectProvider(
                                insetObserver,
                                WindowInsets.Type.captionBar(),
                                WindowInsetsCompat.toWindowInsetsCompat(insets, mCoordinatorView));
        mInsetsRectProvider.addObserver(mInsetsRectUpdateRunnable);

        // Populate the initial value if the rect provider is ready.
        if (!mInsetsRectProvider.getWidestUnoccludedRect().isEmpty()) {
            mInsetsRectUpdateRunnable.onBoundingRectsUpdated(
                    mInsetsRectProvider.getWidestUnoccludedRect());
        }
    }

    /** Destroy the instances and remove all the dependencies. */
    public void destroy() {
        mInsetsRectProvider.removeObserver(mInsetsRectUpdateRunnable);
    }

    /**
     * Returns whether the window this instance is associated with to is in desktop windowing mode.
     */
    public boolean isDesktopWindowingEnabled() {
        return mDesktopWindowingEnabled;
    }

    private void onInsetsRectsUpdated(@NonNull Rect widestUnoccludedRect) {
        if (widestUnoccludedRect.equals(mWidestUnoccludedRect)) return;

        mWidestUnoccludedRect = widestUnoccludedRect;
        boolean desktopWindowingEnabled = isDesktopWindowingModeEnabled();

        // Regardless the current state, we'll update the side padding for StripLayoutHelper, as
        // bounding rect can have updates without entering / exiting desktop windowing mode.
        if (desktopWindowingEnabled) {
            mStripLayoutHelperManager.updateHorizontalPaddings(
                    widestUnoccludedRect.left,
                    mInsetsRectProvider.getWindowRect().width() - widestUnoccludedRect.right);
            updateExclusionRects(mWidestUnoccludedRect);
        } else if (mDesktopWindowingEnabled) {
            // Only reset when we are exiting desktop windowing mode.
            mStripLayoutHelperManager.updateHorizontalPaddings(0, 0);
            updateExclusionRects(null);
        }

        // If whether we are in DW mode does not change, we can end this method now.
        if (desktopWindowingEnabled == mDesktopWindowingEnabled) return;
        mDesktopWindowingEnabled = desktopWindowingEnabled;

        // 1. Enter E2E if we are in desktop windowing mode.
        WindowCompat.setDecorFitsSystemWindows(mActivity.getWindow(), !mDesktopWindowingEnabled);

        // 2. Set the captionBar background appropriately to draw into the region.
        updateCaptionBarBackground(mDesktopWindowingEnabled);
        updateIconColorForCaptionBars();

        // 3. Lock the browser controls when we are in DW mode.
        if (mDesktopWindowingEnabled) {
            mBrowserControlsToken =
                    mBrowserControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mBrowserControlsToken);
        } else {
            mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(mBrowserControlsToken);
        }
    }

    private void updateExclusionRects(Rect rectsSeen) {
        // TODO(crbug/328446763): Get the rect based on tab strip's size.
        mCoordinatorView.setSystemGestureExclusionRects(Collections.singletonList(rectsSeen));
    }

    /**
     * Check if the desktop windowing mode is enabled by checking all the criteria: 1. Caption bar
     * has insets.top > 0 2. Caption bar has 2 bounding rects. 3. Widest unoccluded rect in
     * captionBar insets is connected to the bottom
     */
    // TODO(crbug/328446763): Add metrics to record the failure reason.
    // TODO(crbug/330213938): Add more criteria checks.
    private boolean isDesktopWindowingModeEnabled() {
        int numOfBoundingRects = mInsetsRectProvider.getBoundingRects().size();
        if (numOfBoundingRects != 2) {
            Log.w(TAG, "Unexpected number of bounding rects is observed! " + numOfBoundingRects);
            return false;
        }

        Insets captionBarInset = mInsetsRectProvider.getCachedInset();
        return captionBarInset.top > 0
                && mInsetsRectProvider.getWidestUnoccludedRect().bottom == captionBarInset.top;
    }

    @SuppressLint("WrongConstant")
    private void updateCaptionBarBackground(boolean isTransparent) {
        int captionBarAppearance =
                isTransparent ? APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND : 0;
        int currentCaptionBarAppearance =
                mInsetsController.getSystemBarsAppearance()
                        & APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND;
        // This is a workaround to prevent #setSystemBarsAppearance to trigger infinite inset
        // updates.
        if (currentCaptionBarAppearance != captionBarAppearance) {
            mInsetsController.setSystemBarsAppearance(
                    captionBarAppearance, APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND);
        }
    }

    // TODO(crbug/328446763): Call this method when theme / tab model switches.
    @SuppressLint("WrongConstant")
    private void updateIconColorForCaptionBars() {
        boolean useLightIcon =
                ColorUtils.shouldUseLightForegroundOnBackground(
                        mStripLayoutHelperManager.getBackgroundColor());
        int useLightCaptionBar = useLightIcon ? APPEARANCE_LIGHT_CAPTION_BARS : 0;
        mInsetsController.setSystemBarsAppearance(
                useLightCaptionBar, APPEARANCE_LIGHT_CAPTION_BARS);
    }

    public static void setInsetsRectProviderForTesting(InsetsRectProvider providerForTesting) {
        sInsetsRectProviderForTesting = providerForTesting;
        ResettersForTesting.register(() -> sInsetsRectProviderForTesting = null);
    }
}
