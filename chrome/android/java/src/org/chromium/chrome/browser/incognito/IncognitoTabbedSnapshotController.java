// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.layouts.FilterLayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;

/**
 * This is the controller that prevents incognito tabs from being visible in Android Recents
 * for {@link ChromeTabbedActivity}.
 */
public class IncognitoTabbedSnapshotController extends IncognitoSnapshotController
        implements TabModelSelectorObserver, DestroyObserver {
    private final @NonNull TabModelSelector mTabModelSelector;
    private final @NonNull LayoutManagerChrome mLayoutManager;
    private final @NonNull LayoutStateObserver mLayoutStateObserver;
    private final @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * Creates and registers a new {@link IncognitoTabbedSnapshotController}.
     *
     * @param activity The {@link Activity} on which the snapshot capability needs to be controlled.
     * @param layoutManager The {@link LayoutManagerChrome} where this controller will be added.
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     *     fetched.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} which would allow
     *     to register as {@link DestroyObserver}.
     */
    public static void createIncognitoTabSnapshotController(
            @NonNull Activity activity,
            @NonNull LayoutManagerChrome layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        Supplier<Boolean> isOverviewModeSupplier =
                () -> layoutManager.getActiveLayoutType() == LayoutType.TAB_SWITCHER;
        Supplier<Boolean> isShowingIncognitoSupplier =
                getIsShowingIncognitoSupplier(tabModelSelector, isOverviewModeSupplier);

        new IncognitoTabbedSnapshotController(
                activity,
                layoutManager,
                tabModelSelector,
                activityLifecycleDispatcher,
                isShowingIncognitoSupplier);
    }

    /**
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     *         fetched.
     * @param isInOverviewModeSupplier The {@link Supplier<Boolean>} to supply with the information
     *         whether overview mode is shown or not.
     *
     * @return A {@link Supplier<Boolean>} to supply information about whether incognito is showing
     *         or not.
     */
    @VisibleForTesting
    static Supplier<Boolean> getIsShowingIncognitoSupplier(
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier) {
        return () -> {
            return tabModelSelector.getCurrentModel().isIncognito();
        };
    }

    /**
     * @param activity The {@link Activity} on which the snapshot capability needs to be controlled.
     * @param layoutManager The {@link LayoutManagerChrome} where this controller will be added.
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     *     fetched.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} which would allow
     *     to register as {@link DestroyObserver}.
     * @param isShowingIncognitoSupplier The {@link Supplier<Boolean>} to supply with the
     *     information if we are showing Incognito currently.
     */
    @VisibleForTesting
    IncognitoTabbedSnapshotController(
            @NonNull Activity activity,
            @NonNull LayoutManagerChrome layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isShowingIncognitoSupplier) {
        super(activity, isShowingIncognitoSupplier);

        mLayoutManager = layoutManager;
        mTabModelSelector = tabModelSelector;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;

        mLayoutStateObserver =
                new FilterLayoutStateObserver(
                        LayoutType.TAB_SWITCHER,
                        new LayoutStateObserver() {
                            @Override
                            public void onStartedShowing(int layoutType) {
                                assert layoutType == LayoutType.TAB_SWITCHER;
                                updateIncognitoTabSnapshotState();
                            }

                            @Override
                            public void onStartedHiding(int layoutType) {
                                assert layoutType == LayoutType.TAB_SWITCHER;
                                updateIncognitoTabSnapshotState();
                            }
                        });

        mLayoutManager.addObserver(mLayoutStateObserver);
        mTabModelSelector.addObserver(this);
        mActivityLifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        mLayoutManager.removeObserver(mLayoutStateObserver);
        mTabModelSelector.removeObserver(this);
        mActivityLifecycleDispatcher.unregister(this);
    }

    @Override
    public void onChange() {
        updateIncognitoTabSnapshotState();
    }
}
