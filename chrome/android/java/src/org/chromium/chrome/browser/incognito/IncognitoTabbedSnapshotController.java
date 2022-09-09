// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.content.Context;
import android.view.Window;

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
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

/**
 * This is the controller that prevents incognito tabs from being visible in Android Recents
 * for {@link ChromeTabbedActivity}.
 */
public class IncognitoTabbedSnapshotController
        extends IncognitoSnapshotController implements TabModelSelectorObserver, DestroyObserver {
    private final @NonNull TabModelSelector mTabModelSelector;
    private final @NonNull LayoutManagerChrome mLayoutManager;
    private final @NonNull LayoutStateObserver mLayoutStateObserver;
    private final @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final @NonNull Supplier<Boolean> mIsGTSEnabledSupplier;

    /**
     * Creates and registers a new {@link IncognitoTabbedSnapshotController}.
     * @param context The activity context.
     * @param window The {@link Window} containing the flags to which the secure flag will be added
     *               and cleared.
     * @param layoutManager The {@link LayoutManagerChrome} where this controller will be added.
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     *         fetched.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} which would allow
     *         to register as {@link DestroyObserver}.
     */
    public static void createIncognitoTabSnapshotController(@NonNull Context context,
            @NonNull Window window, @NonNull LayoutManagerChrome layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        Supplier<Boolean> isGTSEnabledSupplier =
                () -> TabUiFeatureUtilities.isGridTabSwitcherEnabled(context);
        Supplier<Boolean> isOverviewModeSupplier =
                () -> layoutManager.getActiveLayoutType() == LayoutType.TAB_SWITCHER;
        Supplier<Boolean> isShowingIncognitoSupplier = getIsShowingIncognitoSupplier(
                tabModelSelector, isGTSEnabledSupplier, isOverviewModeSupplier);

        new IncognitoTabbedSnapshotController(window, layoutManager, tabModelSelector,
                activityLifecycleDispatcher, isGTSEnabledSupplier, isShowingIncognitoSupplier);
    }

    /**
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     *         fetched.
     * @param isGTSEnabledSupplier The {@link Supplier<Boolean>} to supply with the information
     *         whether GTS is enabled or not.
     * @param isInOverviewModeSupplier The {@link Supplier<Boolean>} to supply with the information
     *         whether overview mode is shown or not.
     *
     * @return A {@link Supplier<Boolean>} to supply information about whether incognito is showing
     *         or not.
     */
    @VisibleForTesting
    static Supplier<Boolean> getIsShowingIncognitoSupplier(
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Supplier<Boolean> isGTSEnabledSupplier,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier) {
        return () -> {
            boolean isInIncognitoModel = tabModelSelector.getCurrentModel().isIncognito();

            // If we're using the overlapping tab switcher, we show the edge of the open incognito
            // tabs even if the tab switcher is showing the normal stack. But if the grid tab
            // switcher is enabled, incognito tabs are not visible while we're showing the normal
            // tabs.
            return isInIncognitoModel
                    || (!isGTSEnabledSupplier.get() && isInOverviewModeSupplier.get()
                            && tabModelSelector.getModel(true).getCount() > 0);
        };
    }

    /**
     * @param window The {@link Window} containing the flags to which the secure flag will be added
     *               and cleared.
     * @param layoutManager The {@link LayoutManagerChrome} where this controller will be added.
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     *         fetched.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} which would allow
     *         to register as {@link DestroyObserver}.
     * @param isGTSEnabledSupplier The {@link Supplier<Boolean>} to supply with the information
     *         whether GTS is enabled or not.
     * @param isShowingIncognitoSupplier The {@link Supplier<Boolean>} to supply with the
     *         information if we are showing Incognito currently.
     */
    @VisibleForTesting
    IncognitoTabbedSnapshotController(@NonNull Window window,
            @NonNull LayoutManagerChrome layoutManager, @NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isGTSEnabledSupplier,
            @NonNull Supplier<Boolean> isShowingIncognitoSupplier) {
        super(window, isShowingIncognitoSupplier);

        mLayoutManager = layoutManager;
        mTabModelSelector = tabModelSelector;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mIsGTSEnabledSupplier = isGTSEnabledSupplier;

        mLayoutStateObserver =
                new FilterLayoutStateObserver(LayoutType.TAB_SWITCHER, new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType, boolean showToolbar) {
                        assert layoutType == LayoutType.TAB_SWITCHER;
                        updateIncognitoTabSnapshotState();
                    }

                    @Override
                    public void onStartedHiding(
                            int layoutType, boolean showToolbar, boolean delayAnimation) {
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
