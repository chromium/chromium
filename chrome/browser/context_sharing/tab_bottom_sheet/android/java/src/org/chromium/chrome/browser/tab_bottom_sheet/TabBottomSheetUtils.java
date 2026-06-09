// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;

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
    private static final UnownedUserDataKey<TabBottomSheetManager> MANAGER_KEY =
            new UnownedUserDataKey<>();
    private static final UnownedUserDataKey<CoBrowseViewFactory> FACTORY_KEY =
            new UnownedUserDataKey<>();

    private TabBottomSheetUtils() {}

    public static boolean isTabBottomSheetEnabled() {
        return ChromeFeatureList.sTabBottomSheet.isEnabled();
    }

    public static boolean canResizeWebView() {
        return ChromeFeatureList.sTabBottomSheetResizeWebview.getValue();
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
     * (PAUSED, STOPPED, or DESTROYED). Also returns true if the WindowAndroid is null.
     */
    @Contract("null -> true")
    public static boolean isActivityInactive(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return true;
        @ActivityState int activityState = windowAndroid.getActivityState();
        return activityState == ActivityState.PAUSED
                || activityState == ActivityState.STOPPED
                || activityState == ActivityState.DESTROYED;
    }
}
