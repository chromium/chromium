// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui.close_all_tabs;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.WeakHashMap;

/** Helper for closing all tabs via {@link CloseAllTabsDialog}. */
@NullMarked
public class CloseAllTabsHelper {
    private static final Set<TabModelSelector> sPendingCloseAllSelectors =
            Collections.newSetFromMap(new WeakHashMap<>());

    /** Delegate for preparing and unarchiving archived tabs during a close-all-tabs operation. */
    public interface ArchivedTabsDelegate {
        /**
         * Prepares archived tabs for closure and acquires any needed lifecycle lease.
         *
         * @param onReadyToClose Invoked asynchronously once archived tab state finishes
         *     initializing or is torn down, ONLY when this method returns a non-{@code null}
         *     handle.
         * @return {@code null} if closing can proceed synchronously without invoking {@code
         *     onReadyToClose}, or a {@link Destroyable} handle owning the pending wait and
         *     lifecycle lease for the async operation (cancelling any pending wait and releasing
         *     any held lease when destroyed).
         */
        @Nullable Destroyable prepareForCloseAll(Runnable onReadyToClose);

        /**
         * Unarchives archived tabs into the regular {@link TabModel}, releases any preparation
         * lease, and returns a {@link Runnable} to re-archive them if the closure is undone, or
         * {@code null} if nothing was unarchived. Must execute even when {@code allowUndo} is
         * {@code false}.
         */
        @Nullable Runnable unarchiveAndGetUndoRunnable();
    }

    private CloseAllTabsHelper() {}

    /**
     * Closes all tabs hiding tab groups with an explicit closing source.
     *
     * @param tabModelSelector {@link TabModelSelector} for the activity.
     * @param archivedTabsDelegate Delegate coordinating archived tab readiness and unarchiving.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     * @param closingSource The closing source of the tab closure.
     * @param closePinnedTabs Whether pinned tabs should also be closed when the feature flag is
     *     enabled.
     */
    public static void closeAllTabsHidingTabGroups(
            TabModelSelector tabModelSelector,
            ArchivedTabsDelegate archivedTabsDelegate,
            boolean allowUndo,
            @TabClosingSource int closingSource,
            boolean closePinnedTabs) {
        ThreadUtils.assertOnUiThread();
        if (sPendingCloseAllSelectors.contains(tabModelSelector)) {
            return;
        }
        closeAllIncognitoTabs(
                tabModelSelector, allowUndo, /* hideTabGroups= */ true, closingSource);

        TabModel regularTabModel = tabModelSelector.getModel(/* incognito= */ false);
        if (regularTabModel.getTabModelType() == TabModelType.EMPTY) {
            return;
        }

        new PendingCloseObserver(
                        tabModelSelector,
                        regularTabModel,
                        archivedTabsDelegate,
                        allowUndo,
                        closingSource,
                        closePinnedTabs)
                .start();
    }

    /**
     * Creates a callback to close all tabs, accepting a boolean indicating whether pinned tabs
     * should also be closed.
     *
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param archivedTabsDelegate Delegate coordinating archived tab readiness and unarchiving.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     */
    public static Callback<Boolean> buildCloseAllTabsCallback(
            TabModelSelector tabModelSelector,
            ArchivedTabsDelegate archivedTabsDelegate,
            boolean allowUndo) {
        ThreadUtils.assertOnUiThread();
        return closePinnedTabs ->
                closeAllTabsHidingTabGroups(
                        tabModelSelector,
                        archivedTabsDelegate,
                        allowUndo,
                        TabClosingSource.GRID_TAB_SWITCHER,
                        closePinnedTabs);
    }

    /**
     * Creates a callback to close all incognito tabs; the {@link Boolean} callback parameter is
     * ignored.
     *
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     */
    public static Callback<Boolean> buildCloseAllIncognitoTabsCallback(
            TabModelSelector tabModelSelector, boolean allowUndo) {
        ThreadUtils.assertOnUiThread();
        return _ ->
                closeAllIncognitoTabs(
                        tabModelSelector,
                        allowUndo,
                        /* hideTabGroups= */ false,
                        TabClosingSource.GRID_TAB_SWITCHER);
    }

    private static void completeRegularCloseAll(
            TabModel regularTabModel,
            ArchivedTabsDelegate archivedTabsDelegate,
            boolean allowUndo,
            @TabClosingSource int closingSource,
            boolean closePinnedTabs,
            @Nullable Set<Integer> targetRegularTabIds) {
        Set<Integer> interimTabIds;
        if (targetRegularTabIds != null) {
            interimTabIds = snapshotTabIds(regularTabModel);
            interimTabIds.removeAll(targetRegularTabIds);
        } else {
            interimTabIds = Collections.emptySet();
        }
        // Unarchive archived tabs first so retainPinnedTabs and tab filtering inspect the regular
        // TabModel after all unarchived tabs are restored into it.
        @Nullable Runnable undoRunnable = archivedTabsDelegate.unarchiveAndGetUndoRunnable();

        boolean retainPinnedTabs =
                !closePinnedTabs
                        && ChromeFeatureList.sAndroidCloseAllTabsRetainsPinnedTabs.isEnabled()
                        && regularTabModel.getPinnedTabsCount() > 0;

        TabClosureParams.Builder paramsBuilder;
        if (!retainPinnedTabs && interimTabIds.isEmpty()) {
            paramsBuilder = TabClosureParams.closeAllTabs();
        } else {
            boolean skippedAnyTab = false;
            List<Tab> tabsToClose = new ArrayList<>();
            for (Tab tab : regularTabModel) {
                if (interimTabIds.contains(tab.getId())
                        || (retainPinnedTabs && tab.getIsPinned())) {
                    skippedAnyTab = true;
                } else {
                    tabsToClose.add(tab);
                }
            }
            if (tabsToClose.isEmpty()) {
                return;
            }
            paramsBuilder =
                    skippedAnyTab
                            ? TabClosureParams.closeTabs(tabsToClose)
                            : TabClosureParams.closeAllTabs();
        }

        if (allowUndo && undoRunnable != null) {
            paramsBuilder.withUndoRunnable(undoRunnable);
        }
        regularTabModel
                .getTabRemover()
                .closeTabs(
                        paramsBuilder
                                .hideTabGroups(/* hideTabGroups= */ true)
                                .allowUndo(allowUndo)
                                .tabClosingSource(closingSource)
                                .build(),
                        /* allowDialog= */ false);
    }

    private static Set<Integer> snapshotTabIds(TabModel tabModel) {
        Set<Integer> tabIds = new HashSet<>();
        for (Tab tab : tabModel) {
            tabIds.add(tab.getId());
        }
        return tabIds;
    }

    private static final class PendingCloseObserver implements TabModelSelectorObserver, Runnable {
        private final TabModelSelector mTabModelSelector;
        private final TabModel mInitialRegularTabModel;
        private final ArchivedTabsDelegate mArchivedTabsDelegate;
        private final boolean mAllowUndo;
        private final @TabClosingSource int mClosingSource;
        private final boolean mClosePinnedTabs;
        private @Nullable Set<Integer> mTargetRegularTabIds;
        private @Nullable Destroyable mPreparationHandle;

        PendingCloseObserver(
                TabModelSelector tabModelSelector,
                TabModel initialRegularTabModel,
                ArchivedTabsDelegate archivedTabsDelegate,
                boolean allowUndo,
                @TabClosingSource int closingSource,
                boolean closePinnedTabs) {
            mTabModelSelector = tabModelSelector;
            mInitialRegularTabModel = initialRegularTabModel;
            mArchivedTabsDelegate = archivedTabsDelegate;
            mAllowUndo = allowUndo;
            mClosingSource = closingSource;
            mClosePinnedTabs = closePinnedTabs;
        }

        void start() {
            mPreparationHandle = mArchivedTabsDelegate.prepareForCloseAll(this);
            if (mPreparationHandle == null) {
                completeRegularCloseAll(
                        mInitialRegularTabModel,
                        mArchivedTabsDelegate,
                        mAllowUndo,
                        mClosingSource,
                        mClosePinnedTabs,
                        /* targetRegularTabIds= */ null);
                return;
            }
            mTargetRegularTabIds = snapshotTabIds(mInitialRegularTabModel);
            sPendingCloseAllSelectors.add(mTabModelSelector);
            mTabModelSelector.addObserver(this);
        }

        private @Nullable Destroyable takePendingHandle() {
            ThreadUtils.assertOnUiThread();
            if (mPreparationHandle == null) return null;
            Destroyable handle = mPreparationHandle;
            mPreparationHandle = null;
            sPendingCloseAllSelectors.remove(mTabModelSelector);
            mTabModelSelector.removeObserver(this);
            return handle;
        }

        @Override
        public void onDestroyed() {
            @Nullable Destroyable handle = takePendingHandle();
            if (handle != null) {
                handle.destroy();
            }
        }

        @Override
        public void run() {
            ThreadUtils.assertOnUiThread();
            @Nullable Destroyable handle = takePendingHandle();
            if (handle == null) return;
            try {
                TabModel regularTabModel = mTabModelSelector.getModel(/* incognito= */ false);
                Profile profile = regularTabModel.getProfile();
                if (regularTabModel.getTabModelType() == TabModelType.EMPTY
                        || (profile != null && profile.shutdownStarted())) {
                    return;
                }
                completeRegularCloseAll(
                        regularTabModel,
                        mArchivedTabsDelegate,
                        mAllowUndo,
                        mClosingSource,
                        mClosePinnedTabs,
                        mTargetRegularTabIds);
            } finally {
                handle.destroy();
            }
        }
    }

    static void clearPendingCloseAllSelectorsForTesting() {
        sPendingCloseAllSelectors.clear();
    }

    private static void closeAllIncognitoTabs(
            TabModelSelector tabModelSelector,
            boolean allowUndo,
            boolean hideTabGroups,
            @TabClosingSource int closingSource) {
        ThreadUtils.assertOnUiThread();
        tabModelSelector
                .getModel(/* incognito= */ true)
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .allowUndo(allowUndo)
                                .hideTabGroups(hideTabGroups)
                                .tabClosingSource(closingSource)
                                .build(),
                        /* allowDialog= */ false);
    }
}
