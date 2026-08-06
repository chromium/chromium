// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.WindowAndroid;

/** Utility methods used by the Tab Bottom Sheet components. */
@NullMarked
public final class TabBottomSheetUtils {
    // Values are not final and may need tuning.
    public static final float FULL_HEIGHT_RATIO = 0.7f;
    public static final float SMALL_SCREEN_HEIGHT_RATIO = 0.9f;
    public static final String FULL_HEIGHT_RATIO_PARAM = "full_height_ratio";
    public static final String HALF_HEIGHT_RATIO_PARAM = "half_height_ratio";

    private static final UnownedUserDataKey<TabBottomSheetManager> MANAGER_KEY =
            new UnownedUserDataKey<>();
    private static final UnownedUserDataKey<CoBrowseViewFactory> FACTORY_KEY =
            new UnownedUserDataKey<>();

    private TabBottomSheetUtils() {}

    public static boolean isTabBottomSheetEnabled() {
        return ChromeFeatureList.sTabBottomSheet.isEnabled();
    }

    public static boolean canResizeWebView() {
        return isTabBottomSheetEnabled()
                && ChromeFeatureList.sTabBottomSheetResizeWebview.isEnabled();
    }

    /** Returns the full height ratio for the Tab Bottom Sheet. */
    public static float getFullHeightRatio() {
        if (ChromeFeatureList.sTabBottomSheetFullHeight.isEnabled()) {
            return (float)
                    ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                            ChromeFeatureList.TAB_BOTTOM_SHEET_FULL_HEIGHT,
                            FULL_HEIGHT_RATIO_PARAM,
                            FULL_HEIGHT_RATIO);
        }
        return FULL_HEIGHT_RATIO;
    }

    /**
     * Returns the default height ratio for the Tab Bottom Sheet.
     *
     * @param context The {@link Context} to retrieve configuration from.
     * @param isKeyboardShowing Whether the soft keyboard is currently showing.
     */
    public static float getDefaultHeightRatio(Context context, boolean isKeyboardShowing) {
        Configuration configuration = context.getResources().getConfiguration();
        if (configuration.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            return SMALL_SCREEN_HEIGHT_RATIO;
        }
        return isKeyboardShowing ? SMALL_SCREEN_HEIGHT_RATIO : getFullHeightRatio();
    }

    /**
     * Attach TabBottomSheetManager to WindowAndroid. This allows TabBottomSheetManager to be
     * retrieved statically.
     *
     * @param windowAndroid The {@link WindowAndroid} to attach to.
     * @param manager The {@link TabBottomSheetManager} to attach.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void attachManagerToWindow(
            WindowAndroid windowAndroid, TabBottomSheetManager manager) {
        MANAGER_KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), manager);
    }

    /**
     * Detach TabBottomSheetManager from WindowAndroid.
     *
     * @param windowAndroid The {@link WindowAndroid} to detach from.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void detachManagerFromWindow(WindowAndroid windowAndroid) {
        MANAGER_KEY.detachFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Returns the {@link TabBottomSheetManager} associated with the given {@link WindowAndroid}.
     *
     * @param windowAndroid The {@link WindowAndroid} to retrieve the manager from.
     * @return The {@link TabBottomSheetManager}, or null if not found.
     */
    public static @Nullable TabBottomSheetManager getManagerFromWindow(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) {
            return null;
        }
        return MANAGER_KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attach CoBrowseViewFactory to WindowAndroid. This allows CoBrowseViewFactory to be retrieved
     * statically.
     *
     * @param windowAndroid The {@link WindowAndroid} to attach to.
     * @param factory The {@link CoBrowseViewFactory} to attach.
     */
    static void attachFactoryToWindow(WindowAndroid windowAndroid, CoBrowseViewFactory factory) {
        FACTORY_KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), factory);
    }

    /**
     * Detach CoBrowseViewFactory from WindowAndroid.
     *
     * @param windowAndroid The {@link WindowAndroid} to detach from.
     */
    static void detachFactoryFromWindow(WindowAndroid windowAndroid) {
        FACTORY_KEY.detachFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Returns the {@link CoBrowseViewFactory} associated with the given {@link WindowAndroid}.
     *
     * @param windowAndroid The {@link WindowAndroid} to retrieve the factory from.
     * @return The {@link CoBrowseViewFactory}, or null if not found.
     */
    static @Nullable CoBrowseViewFactory getFactoryFromWindow(WindowAndroid windowAndroid) {
        return FACTORY_KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Returns whether the Activity associated with the given WindowAndroid is finishing or
     * destroyed. Also returns true if the WindowAndroid or the Activity is null.
     */
    @Contract("null -> true")
    public static boolean isActivityFinishingOrDestroyed(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return true;
        Activity activity = windowAndroid.getActivity().get();
        return activity == null || activity.isDestroyed() || activity.isFinishing();
    }

    /**
     * Returns whether the Activity associated with the given WindowAndroid is in an inactive state
     * (STOPPED or DESTROYED). Also returns true if the WindowAndroid is null.
     */
    @Contract("null -> true")
    public static boolean isActivityInactive(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return true;
        @ActivityState int activityState = windowAndroid.getActivityState();
        return activityState == ActivityState.STOPPED || activityState == ActivityState.DESTROYED;
    }
}
