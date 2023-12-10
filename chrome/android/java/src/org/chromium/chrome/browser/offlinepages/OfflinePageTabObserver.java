// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/**
 * A class that observes events for a tab which has an associated offline page. It is created when
 * the first offline page is loaded in any tab. When additional offline pages are opened, they are
 * all watched by the same observer. This observer will decide when to show a reload snackbar for
 * those tabs. The following conditions need to be met to show the snackbar:
 * <ul>
 *   <li>Tab has to be shown,</li>
 *   <li>Offline page has to be loaded,</li>
 *   <li>Chrome is connected to the web,</li>
 *   <li>Unless triggering condition is change in network, snackbar hasn't been shown for that
 *   tab.</li>
 * </ul>
 * When the last tab with offline page is closed or navigated away from, this observer stops
 * listening to network changes.
 */
public class OfflinePageTabObserver extends EmptyTabObserver
        implements NetworkChangeNotifier.ConnectionTypeObserver {
    private static final String TAG = "OfflinePageTO";

    /** Class for keeping the state of observed tabs. */
    private static class TabState {
        /** Whether content in a tab finished loading. */
        public boolean isLoaded;

        /** Whether a snackbar was shown for the tab. */
        public boolean wasSnackbarSeen;

        public TabState(boolean isLoaded) {
            this.isLoaded = isLoaded;
            this.wasSnackbarSeen = false;
        }
    }

    private static Map<Activity, OfflinePageTabObserver> sObservers;

    private final SnackbarManager mSnackbarManager;
    private final SnackbarController mSnackbarController;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorTabModelObserver mTabModelObserver;

    /** Map of observed tabs. */
    private final Map<Integer, TabState> mObservedTabs = new HashMap<>();

    private boolean mIsObservingNetworkChanges;

    /** Current tab, kept track of for the network change notification. */
    private Tab mCurrentTab;

    private static OfflinePageTabObserver getObserverForWindowAndroid(WindowAndroid windowAndroid) {
        ensureObserverMapInitialized();
        Activity activity = windowAndroid.getActivity().get();
        OfflinePageTabObserver observer = sObservers.get(activity);
        if (observer == null) {
            TabModelSelector tabModelSelector = TabModelSelectorSupplier.from(windowAndroid).get();
            observer =
                    new OfflinePageTabObserver(
                            tabModelSelector,
                            SnackbarManagerProvider.from(windowAndroid),
                            createReloadSnackbarController(tabModelSelector));
            sObservers.put(activity, observer);
        }
        return observer;
    }

    private static void ensureObserverMapInitialized() {
        if (sObservers != null) return;
        sObservers = new HashMap<>();
        ApplicationStatus.registerStateListenerForAllActivities(
                new ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        if (newState != ActivityState.DESTROYED) return;
                        OfflinePageTabObserver observer = sObservers.remove(activity);
                        if (observer == null) return;
                        observer.destroy();
                    }
                });
    }

    static void setObserverForTesting(Activity activity, OfflinePageTabObserver observer) {
        ensureObserverMapInitialized();
        sObservers.put(activity, observer);
    }

    /**
     * Create and attach a tab observer if we don't already have one, otherwise update it.
     * @param tab The tab we are adding an observer for.
     */
    public static void addObserverForTab(Tab tab) {
        OfflinePageTabObserver observer = getObserverForWindowAndroid(tab.getWindowAndroid());
        observer.startObservingTab(tab);
        observer.maybeShowReloadSnackbar(tab, false);
    }

    /**
     * Builds a new OfflinePageTabObserver.
     * @param tabModelSelector Tab model selector for the activity.
     * @param snackbarManager The snackbar manager to show and dismiss snackbars.
     * @param snackbarController Controller to use to build the snackbar.
     */
    OfflinePageTabObserver(
            TabModelSelector tabModelSelector,
            SnackbarManager snackbarManager,
            SnackbarController snackbarController) {
        mSnackbarManager = snackbarManager;
        mSnackbarController = snackbarController;
        mTabModelSelector = tabModelSelector;
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(tabModelSelector) {
                    @Override
                    public void tabRemoved(Tab tab) {
                        Log.d(TAG, "tabRemoved");
                        stopObservingTab(tab);
                        mSnackbarManager.dismissSnackbars(mSnackbarController);
                    }
                };
        // The first time observer is created snackbar has net yet been shown.
        mIsObservingNetworkChanges = false;
    }

    // Methods from EmptyTabObserver
    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        Log.d(TAG, "onPageLoadFinished");
        if (isObservingTab(tab)) {
            mObservedTabs.get(tab.getId()).isLoaded = true;
            maybeShowReloadSnackbar(tab, false);
        }
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        Log.d(TAG, "onShow");
        maybeShowReloadSnackbar(tab, false);
        mCurrentTab = tab;
    }

    @Override
    public void onHidden(Tab hiddenTab, @TabHidingType int type) {
        Log.d(TAG, "onHidden");
        mCurrentTab = null;
        mSnackbarManager.dismissSnackbars(mSnackbarController);
    }

    @Override
    public void onDestroyed(Tab tab) {
        Log.d(TAG, "onDestroyed");
        stopObservingTab(tab);
        mSnackbarManager.dismissSnackbars(mSnackbarController);
    }

    @Override
    public void onUrlUpdated(Tab tab) {
        Log.d(TAG, "onUrlUpdated");
        if (!OfflinePageUtils.isOfflinePage(tab)) {
            stopObservingTab(tab);
        } else if (isObservingTab(tab)) {
            mObservedTabs.get(tab.getId()).isLoaded = false;
            mObservedTabs.get(tab.getId()).wasSnackbarSeen = false;
        }
        // In case any snackbars are showing, dismiss them before we navigate away.
        mSnackbarManager.dismissSnackbars(mSnackbarController);
    }

    void startObservingTab(Tab tab) {
        assert tab.isInitialized();
        boolean isOfflinePage = OfflinePageUtils.isOfflinePage(tab);
        // Cache the offline state of the tab so we don't have to go to native every time we want to
        // check this.
        OfflinePageTabData offlinePageTabData = OfflinePageTabData.from(tab);
        offlinePageTabData.setIsTabShowingOfflinePage(isOfflinePage);
        offlinePageTabData.setIsTabShowingTrustedOfflinePage(
                OfflinePageUtils.isShowingTrustedOfflinePage(tab.getWebContents()));

        if (!isOfflinePage) return;

        mCurrentTab = tab;

        // If we are not observing the tab yet, let's.
        if (!isObservingTab(tab)) {
            // Adding a tab happens from inside of onPageLoadFinished, therefore if this is the time
            // we start observing the tab, the page inside of it is already loaded.
            mObservedTabs.put(tab.getId(), new TabState(true));
            tab.addObserver(this);
        }

        // If we are not observing network changes yet, let's.
        if (!isObservingNetworkChanges()) {
            startObservingNetworkChanges();
            mIsObservingNetworkChanges = true;
        }
    }

    /**
     * Removes the observer for a tab with the specified tabId.
     * @param tab tab that was observed.
     */
    void stopObservingTab(Tab tab) {
        // If we are observing the tab, stop.
        if (isObservingTab(tab)) {
            assert tab.isInitialized();
            // Reset the cached offline state of the tab so we don't have to go to native every time
            // we want to check this.
            OfflinePageTabData offlinePageTabData = OfflinePageTabData.from(tab);
            offlinePageTabData.setIsTabShowingOfflinePage(false);
            offlinePageTabData.setIsTabShowingTrustedOfflinePage(false);

            mObservedTabs.remove(tab.getId());
            tab.removeObserver(this);
        }

        // If there are not longer any tabs being observed, stop listening for network changes.
        if (mObservedTabs.isEmpty() && isObservingNetworkChanges()) {
            stopObservingNetworkChanges();
            mIsObservingNetworkChanges = false;
        }
    }

    private void destroy() {
        mTabModelObserver.destroy();
        if (!mObservedTabs.isEmpty()) {
            for (Integer tabId : mObservedTabs.keySet()) {
                Tab tab = mTabModelSelector.getTabById(tabId);
                if (tab == null) continue;
                tab.removeObserver(this);
            }
            mObservedTabs.clear();
        }
        if (isObservingNetworkChanges()) {
            stopObservingNetworkChanges();
            mIsObservingNetworkChanges = false;
        }
    }

    // Methods from ConnectionTypeObserver.
    @Override
    public void onConnectionTypeChanged(int connectionType) {
        Log.d(
                TAG,
                "Got connectivity event, connectionType: "
                        + connectionType
                        + ", is connected: "
                        + OfflinePageUtils.isConnected()
                        + ", controller: "
                        + mSnackbarController);
        maybeShowReloadSnackbar(mCurrentTab, true);

        // Since we are loosing the connection, next time we connect, we still want to show a
        // snackbar. This works in event that onConnectionTypeChanged happens, while Chrome is not
        // visible. Making it visible after that would not trigger the snackbar, even though
        // connection state changed. See http://crbug.com/651410
        if (!OfflinePageUtils.isConnected()) {
            for (TabState tabState : mObservedTabs.values()) {
                tabState.wasSnackbarSeen = false;
            }
        }
    }

    @VisibleForTesting
    boolean isObservingTab(Tab tab) {
        return mObservedTabs.containsKey(tab.getId());
    }

    @VisibleForTesting
    boolean isLoadedTab(Tab tab) {
        return isObservingTab(tab) && mObservedTabs.get(tab.getId()).isLoaded;
    }

    @VisibleForTesting
    boolean wasSnackbarSeen(Tab tab) {
        return isObservingTab(tab) && mObservedTabs.get(tab.getId()).wasSnackbarSeen;
    }

    @VisibleForTesting
    boolean isObservingNetworkChanges() {
        return mIsObservingNetworkChanges;
    }

    void maybeShowReloadSnackbar(Tab tab, boolean isNetworkEvent) {
        // Exclude Offline Previews, as there is a seperate UI for previews.
        if (tab == null
                || tab.isFrozen()
                || tab.isHidden()
                || !OfflinePageUtils.isOfflinePage(tab)
                || OfflinePageUtils.isShowingOfflinePreview(tab)
                || !OfflinePageUtils.isConnected()
                || !isLoadedTab(tab)
                || (wasSnackbarSeen(tab) && !isNetworkEvent)) {
            // Conditions to show a snackbar are not met.
            return;
        }

        showReloadSnackbar(tab);
        mObservedTabs.get(tab.getId()).wasSnackbarSeen = true;
    }

    @VisibleForTesting
    void showReloadSnackbar(Tab tab) {
        OfflinePageUtils.showReloadSnackbar(
                tab.getContext(), mSnackbarManager, mSnackbarController, tab.getId());
    }

    @VisibleForTesting
    void startObservingNetworkChanges() {
        NetworkChangeNotifier.addConnectionTypeObserver(this);
    }

    @VisibleForTesting
    void stopObservingNetworkChanges() {
        NetworkChangeNotifier.removeConnectionTypeObserver(this);
    }

    @VisibleForTesting
    TabModelObserver getTabModelObserver() {
        return mTabModelObserver;
    }

    /**
     * Gets a snackbar controller that we can use to show our snackbar.
     * @param tabModelSelector used to retrieve a tab by ID
     */
    private static SnackbarController createReloadSnackbarController(
            final TabModelSelector tabModelSelector) {
        Log.d(TAG, "building snackbar controller");

        return new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                assert actionData != null;
                int tabId = (int) actionData;
                RecordUserAction.record("OfflinePages.ReloadButtonClicked");
                Tab foundTab = tabModelSelector.getTabById(tabId);
                if (foundTab == null) return;
                if (!OfflinePageUtils.isShowingTrustedOfflinePage(foundTab.getWebContents())) {
                    RecordUserAction.record("OfflinePages.ReloadButtonClickedViewingUntrustedPage");
                }
                // Delegates to Tab to reload the page. Tab will send the correct header in order to
                // load the right page.
                foundTab.reload();
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("OfflinePages.ReloadButtonNotClicked");
            }
        };
    }
}
