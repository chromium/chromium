// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_windowing;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.toolbar.top.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderState;
import org.chromium.chrome.browser.ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetsRectProvider;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

/**
 * Class coordinating the business logic to draw into app header in desktop windowing mode, ranging
 * from listening the window insets updates, and pushing updates to the tab strip.
 */
@RequiresApi(api = Build.VERSION_CODES.R)
public class AppHeaderCoordinator implements DesktopWindowStateProvider {
    @VisibleForTesting
    public static final String INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW =
            "is_app_in_unfocused_desktop_window";

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
    private final InsetObserver mInsetObserver;
    private final InsetsRectProvider mInsetsRectProvider;
    private final WindowInsetsController mInsetsController;
    private final OneshotSupplier<AppHeaderDelegate> mAppHeaderDelegateSupplier;
    private final OneshotSupplier<TabStripTransitionCoordinator>
            mTabStripTransitionCoordinatorSupplier;
    private final ObserverList<AppHeaderObserver> mObservers = new ObserverList<>();
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // Internal states
    private boolean mIsInDesktopWindow;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
    private @Nullable AppHeaderState mAppHeaderState;
    private boolean mIsInUnfocusedDesktopWindow;

    /**
     * Instantiate the coordinator to handle drawing the tab strip into the captionBar area.
     *
     * @param activity The activity associated with the window containing the app header.
     * @param rootView The root view within the activity.
     * @param browserControlsVisibilityDelegate Delegate interface allowing control of the browser
     *     controls visibility.
     * @param insetObserver {@link InsetObserver} that manages insets changes on the
     *     CoordinatorView.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} to dispatch {@link
     *     TopResumedActivityChangedObserver#onTopResumedActivityChanged(boolean)} and {@link
     *     SaveInstanceStateObserver#onSaveInstanceState(Bundle)} events observed by this class.
     * @param savedInstanceState The saved instance state {@link Bundle} holding UI state
     *     information for restoration on startup.
     */
    @SuppressWarnings("WrongConstant")
    public AppHeaderCoordinator(
            Activity activity,
            View rootView,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            InsetObserver insetObserver,
            OneshotSupplier<AppHeaderDelegate> appHeaderDelegateSupplier,
            OneshotSupplier<TabStripTransitionCoordinator> tabStripTransitionCoordinatorSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Bundle savedInstanceState) {
        mActivity = activity;
        mRootView = rootView;
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
        mInsetObserver = insetObserver;
        mInsetsController = mRootView.getWindowInsetsController();
        mAppHeaderDelegateSupplier = appHeaderDelegateSupplier;
        mTabStripTransitionCoordinatorSupplier = tabStripTransitionCoordinatorSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        // Whether the app started in an unfocused desktop window, so that relevant UI state can be
        // restored.
        mIsInUnfocusedDesktopWindow =
                savedInstanceState != null
                        && savedInstanceState.getBoolean(
                                INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW, false);

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
                (delegate) -> maybeUpdateAppHeaderPaddings());
        mTabStripTransitionCoordinatorSupplier.runSyncOrOnAvailable(
                (tabStripTransitionCoordinator) ->
                        tabStripTransitionCoordinator.setInsetRectProvider(mInsetsRectProvider));
    }

    /** Destroy the instances and remove all the dependencies. */
    public void destroy() {
        mActivity = null;
        mInsetsRectProvider.destroy();
        mObservers.clear();
        if (mTabStripTransitionCoordinatorSupplier.get() != null) {
            mTabStripTransitionCoordinatorSupplier.get().setInsetRectProvider(null);
        }
        mActivityLifecycleDispatcher.unregister(this);
    }

    @Override
    public AppHeaderState getAppHeaderState() {
        return mAppHeaderState;
    }

    // TODO(crbug.com/337086192): Read from mAppHeaderState.
    @Override
    public boolean isInDesktopWindow() {
        return mIsInDesktopWindow;
    }

    @Override
    public boolean isInUnfocusedDesktopWindow() {
        return mIsInUnfocusedDesktopWindow;
    }

    @Override
    public boolean addObserver(AppHeaderObserver observer) {
        return mObservers.addObserver(observer);
    }

    @Override
    public boolean removeObserver(AppHeaderObserver observer) {
        return mObservers.removeObserver(observer);
    }

    // TopResumedActivityChangedObserver implementation.
    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        mIsInUnfocusedDesktopWindow = !isTopResumedActivity && mIsInDesktopWindow;
    }

    // SaveInstanceStateObserver implementation.
    @Override
    public void onSaveInstanceState(Bundle outState) {
        outState.putBoolean(INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW, mIsInUnfocusedDesktopWindow);
    }

    private void onInsetsRectsUpdated(@NonNull Rect widestUnoccludedRect) {
        boolean isInDesktopWindow =
                checkIsInDesktopWindow(mActivity, mInsetObserver, mInsetsRectProvider);
        // Use an empty |widestUnoccludedRect| instead of the cached Rect while creating the
        // AppHeaderState while not in or while exiting desktop windowing mode, so that it always
        // holds a valid state for observers to use.
        var appHeaderState =
                new AppHeaderState(
                        mInsetsRectProvider.getWindowRect(),
                        isInDesktopWindow
                                ? mInsetsRectProvider.getWidestUnoccludedRect()
                                : new Rect(),
                        isInDesktopWindow);
        if (appHeaderState.equals(mAppHeaderState)) return;

        boolean desktopWindowingModeChanged = mIsInDesktopWindow != isInDesktopWindow;
        mIsInDesktopWindow = isInDesktopWindow;
        mAppHeaderState = appHeaderState;
        for (var observer : mObservers) {
            observer.onAppHeaderStateChanged(mAppHeaderState);
        }

        // Regardless the current state, we'll update the side padding for StripLayoutHelper, as
        // bounding rect can have updates without entering / exiting desktop windowing mode.
        maybeUpdateAppHeaderPaddings();

        // If whether we are in DW mode does not change, we can end this method now.
        if (!desktopWindowingModeChanged) return;
        for (var observer : mObservers) {
            observer.onDesktopWindowingModeChanged(mIsInDesktopWindow);
        }

        // 1. Enter E2E if we are in desktop windowing mode.
        WindowCompat.setDecorFitsSystemWindows(mActivity.getWindow(), !mIsInDesktopWindow);

        // 2. Set the captionBar background appropriately to draw into the region.
        updateCaptionBarBackground(mIsInDesktopWindow);
        updateIconColorForCaptionBars();

        // 3. Lock the browser controls when we are in DW mode.
        if (mIsInDesktopWindow) {
            mBrowserControlsToken =
                    mBrowserControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mBrowserControlsToken);
        } else {
            mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(mBrowserControlsToken);
        }
    }

    private void maybeUpdateAppHeaderPaddings() {
        if (mAppHeaderDelegateSupplier.get() == null
                || mInsetsRectProvider.getWindowRect().isEmpty()
                || mAppHeaderState == null) return;

        if (mAppHeaderState.isInDesktopWindow()) {
            mAppHeaderDelegateSupplier
                    .get()
                    .updateHorizontalPaddings(
                            mAppHeaderState.getLeftPadding(), mAppHeaderState.getRightPadding());
        } else {
            mAppHeaderDelegateSupplier.get().updateHorizontalPaddings(0, 0);
        }
    }

    /**
     * Check if the desktop windowing mode is enabled by checking all the criteria:
     *
     * <ol type=1>
     *   <li>Caption bar has insets.top > 0;
     *   <li>There's no bottom insets from the navigation bar;
     *   <li>Caption bar has 2 bounding rects;
     *   <li>Widest unoccluded rect in captionBar insets is connected to the bottom;
     * </ol>
     *
     * This method is marked as static, in order to ensure it does not change / read any state from
     * an AppHeaderCoordinator instance, especially the cached {@link AppHeaderState}.
     */
    // TODO(crbug/328446763): Add metrics to record the failure reason.
    private static boolean checkIsInDesktopWindow(
            Activity activity, InsetObserver insetObserver, InsetsRectProvider insetsRectProvider) {

        if (!activity.isInMultiWindowMode()) return false;

        // Disable DW mode if there is a navigation bar (though it may or may not be visible /
        // dismissed).
        assert insetObserver.getLastRawWindowInsets() != null
                : "Attempt to read the insets too early.";

        var navBarInsets =
                insetObserver
                        .getLastRawWindowInsets()
                        .getInsets(WindowInsetsCompat.Type.navigationBars());
        if (navBarInsets.bottom > 0) {
            return false;
        }

        int numOfBoundingRects = insetsRectProvider.getBoundingRects().size();
        if (numOfBoundingRects != 2) {
            Log.w(TAG, "Unexpected number of bounding rects is observed! " + numOfBoundingRects);
            return false;
        }

        Insets captionBarInset = insetsRectProvider.getCachedInset();
        return captionBarInset.top > 0
                && insetsRectProvider.getWidestUnoccludedRect().bottom == captionBarInset.top;
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

    /** Set states for testing. */
    public void setStateForTesting(boolean isInDesktopWindow, AppHeaderState appHeaderState) {
        mIsInDesktopWindow = isInDesktopWindow;
        mAppHeaderState = appHeaderState;

        for (var observer : mObservers) {
            observer.onAppHeaderStateChanged(mAppHeaderState);
            observer.onDesktopWindowingModeChanged(mIsInDesktopWindow);
        }
    }
}
