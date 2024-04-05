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
import androidx.annotation.Px;
import androidx.annotation.RequiresApi;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.toolbar.top.TabStripTransitionCoordinator;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetsRectProvider;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

/**
 * Class coordinating the business logic to draw into app header in desktop windowing mode, ranging
 * from listening the window insets updates, and pushing updates to the tab strip.
 */
@RequiresApi(api = Build.VERSION_CODES.R)
public class AppHeaderCoordinator extends ObservableSupplierImpl<Boolean> {
    private static final String TAG = "AppHeader";
    // TODO(crbug/328446763): Use values from Android V and remove SuppressWarnings.
    private static final int APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND = 1 << 7;
    private static final int APPEARANCE_LIGHT_CAPTION_BARS = 1 << 8;

    private static @Nullable InsetsRectProvider sInsetsRectProviderForTesting;

    /** External delegate to adjust UI in response to app header signals. */
    public interface AppHeaderDelegate {

        /**
         * Adjust the paddings for app header region.
         *
         * @param leftPadding Left padding at the app header region in px.
         * @param rightPadding Right padding at the app header region in px.
         */
        void updateHorizontalPaddings(@Px int leftPadding, @Px int rightPadding);

        /**
         * @return The background color to be used for the app header.
         */
        int getAppHeaderBackgroundColor();
    }

    private Activity mActivity;
    private final View mRootView;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final InsetsRectProvider mInsetsRectProvider;
    private final WindowInsetsController mInsetsController;
    private final OneshotSupplier<AppHeaderDelegate> mAppHeaderDelegateSupplier;
    private final OneshotSupplier<TabStripTransitionCoordinator>
            mTabStripTransitionCoordinatorSupplier;

    // Internal states
    private boolean mDesktopWindowingEnabled;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;

    private Rect mWidestUnoccludedRect;

    /**
     * Instantiate the coordinator to handle drawing the tab strip into the captionBar area.
     *
     * @param activity The activity associated with the window containing the app header.
     * @param rootView The root view within the activity.
     * @param browserControlsVisibilityDelegate Delegate interface allowing control of the browser
     *     controls visibility.
     * @param insetObserver {@link InsetObserver} that manages insets changes on the
     *     CoordinatorView.
     */
    @SuppressWarnings("WrongConstant")
    public AppHeaderCoordinator(
            Activity activity,
            View rootView,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            InsetObserver insetObserver,
            OneshotSupplier<AppHeaderDelegate> appHeaderDelegateSupplier,
            OneshotSupplier<TabStripTransitionCoordinator> tabStripTransitionCoordinatorSupplier) {
        mActivity = activity;
        mRootView = rootView;
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
        mInsetsController = mRootView.getWindowInsetsController();
        mAppHeaderDelegateSupplier = appHeaderDelegateSupplier;
        mTabStripTransitionCoordinatorSupplier = tabStripTransitionCoordinatorSupplier;

        // Initialize mInsetsRectProvider and setup observers.
        WindowInsets insets = mRootView.getRootWindowInsets();
        WindowInsetsCompat initInsets =
                insets == null ? null : WindowInsetsCompat.toWindowInsetsCompat(insets, mRootView);
        mInsetsRectProvider =
                sInsetsRectProviderForTesting != null
                        ? sInsetsRectProviderForTesting
                        : new InsetsRectProvider(
                                insetObserver, WindowInsets.Type.captionBar(), initInsets);
        InsetsRectProvider.Observer insetsRectUpdateRunnable = this::onInsetsRectsUpdated;
        mInsetsRectProvider.addObserver(insetsRectUpdateRunnable);

        // Populate the initial value if the rect provider is ready.
        if (!mInsetsRectProvider.getWidestUnoccludedRect().isEmpty()) {
            insetsRectUpdateRunnable.onBoundingRectsUpdated(
                    mInsetsRectProvider.getWidestUnoccludedRect());
        }

        mAppHeaderDelegateSupplier.runSyncOrOnAvailable(
                (delegate) -> maybeUpdateAppHeaderPaddings(mDesktopWindowingEnabled));
        mTabStripTransitionCoordinatorSupplier.runSyncOrOnAvailable(
                (tabStripTransitionCoordinator) ->
                        tabStripTransitionCoordinator.setInsetRectProvider(mInsetsRectProvider));
        set(mDesktopWindowingEnabled);
    }

    /** Destroy the instances and remove all the dependencies. */
    public void destroy() {
        mActivity = null;
        mInsetsRectProvider.destroy();
        if (mTabStripTransitionCoordinatorSupplier.get() != null) {
            mTabStripTransitionCoordinatorSupplier.get().setInsetRectProvider(null);
        }
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
        maybeUpdateAppHeaderPaddings(desktopWindowingEnabled);

        // If whether we are in DW mode does not change, we can end this method now.
        if (desktopWindowingEnabled == mDesktopWindowingEnabled) return;
        mDesktopWindowingEnabled = desktopWindowingEnabled;
        set(mDesktopWindowingEnabled);

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

    private void maybeUpdateAppHeaderPaddings(boolean isDesktopWindowingEnabled) {
        if (mAppHeaderDelegateSupplier.get() == null
                || mInsetsRectProvider.getWindowRect().isEmpty()
                || mWidestUnoccludedRect == null) return;

        if (isDesktopWindowingEnabled) {
            mAppHeaderDelegateSupplier
                    .get()
                    .updateHorizontalPaddings(
                            mWidestUnoccludedRect.left,
                            mInsetsRectProvider.getWindowRect().width()
                                    - mWidestUnoccludedRect.right);
        } else if (mDesktopWindowingEnabled) {
            // Only reset when we are exiting desktop windowing mode.
            mAppHeaderDelegateSupplier.get().updateHorizontalPaddings(0, 0);
        }
    }

    /**
     * Check if the desktop windowing mode is enabled by checking all the criteria: 1. Caption bar
     * has insets.top > 0 2. Caption bar has 2 bounding rects. 3. Widest unoccluded rect in
     * captionBar insets is connected to the bottom
     */
    // TODO(crbug/328446763): Add metrics to record the failure reason.
    // TODO(crbug/330213938): Add more criteria checks.
    private boolean isDesktopWindowingModeEnabled() {
        if (!mActivity.isInMultiWindowMode()) return false;

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
        if (mAppHeaderDelegateSupplier.get() == null) return;

        boolean useLightIcon =
                ColorUtils.shouldUseLightForegroundOnBackground(
                        mAppHeaderDelegateSupplier.get().getAppHeaderBackgroundColor());
        int useLightCaptionBar = useLightIcon ? APPEARANCE_LIGHT_CAPTION_BARS : 0;
        mInsetsController.setSystemBarsAppearance(
                useLightCaptionBar, APPEARANCE_LIGHT_CAPTION_BARS);
    }

    public static void setInsetsRectProviderForTesting(InsetsRectProvider providerForTesting) {
        sInsetsRectProviderForTesting = providerForTesting;
        ResettersForTesting.register(() -> sInsetsRectProviderForTesting = null);
    }
}
