// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_NEW;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;

/** Unit tests for OfflinePageUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OfflinePageTabObserverTest {
    // Using a null tab, as it cannot be mocked. TabHelper will help return proper mocked responses.
    private static final int TAB_ID = 77;
    private static final GURL TAB_URL = JUnitTestGURLs.EXAMPLE_URL;
    @Mock private ChromeActivity mActivity;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private SnackbarController mSnackbarController;
    @Mock private Tab mTab;
    @Mock private OfflinePageUtils.Internal mOfflinePageUtils;
    @Mock private WindowAndroid mWindowAndroid;
    private WeakReference<Activity> mActivityRef;

    private OfflinePageTabObserver createObserver() {
        OfflinePageTabObserver observer =
                spy(
                        new OfflinePageTabObserver(
                                mTabModelSelector, mSnackbarManager, mSnackbarController));
        // Mocking out all of the calls that touch on NetworkChangeNotifier, which we cannot
        // directly mock out.
        doNothing().when(observer).startObservingNetworkChanges();
        doNothing().when(observer).stopObservingNetworkChanges();

        // Assert tab model observer was created.
        assertTrue(observer.getTabModelObserver() != null);
        return observer;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityRef = new WeakReference<>(mActivity);

        // Setting up a mock tab. These are the values common to most tests, but individual
        // tests might easily overwrite them.
        doReturn(TAB_ID).when(mTab).getId();
        doReturn(TAB_URL).when(mTab).getUrl();
        doReturn(false).when(mTab).isFrozen();
        doReturn(false).when(mTab).isHidden();
        doReturn(mWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(true).when(mTab).isInitialized();
        doReturn(mActivityRef).when(mWindowAndroid).getActivity();

        UserDataHost userDataHost = new UserDataHost();
        OfflinePageTabData offlinePageTabData = new OfflinePageTabData();
        userDataHost.setUserData(OfflinePageTabData.class, offlinePageTabData);
        doReturn(userDataHost).when(mTab).getUserDataHost();

        // Setting up mock snackbar manager.
        doNothing().when(mSnackbarManager).dismissSnackbars(eq(mSnackbarController));

        // Setting up offline page utils.
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        doReturn(false).when(mOfflinePageUtils).isConnected();
        doReturn(false).when(mOfflinePageUtils).isShowingOfflinePreview(any(Tab.class));
        doReturn(true).when(mOfflinePageUtils).isOfflinePage(any(Tab.class));
        doNothing()
                .when(mOfflinePageUtils)
                .showReloadSnackbar(
                        any(Context.class),
                        any(SnackbarManager.class),
                        any(SnackbarController.class),
                        anyInt());
    }

    private void showTab(OfflinePageTabObserver observer) {
        doReturn(false).when(mTab).isHidden();
        if (observer != null) {
            observer.onShown(mTab, FROM_NEW);
        }
    }

    private void hideTab(OfflinePageTabObserver observer) {
        doReturn(true).when(mTab).isHidden();
        if (observer != null) {
            observer.onHidden(mTab, TabHidingType.CHANGED_TABS);
        }
    }

    private void removeTab(OfflinePageTabObserver observer) {
        if (observer != null && observer.getTabModelObserver() != null) {
            observer.getTabModelObserver().tabRemoved(mTab);
        }
    }

    private void connect(OfflinePageTabObserver observer, boolean notify) {
        doReturn(true).when(mOfflinePageUtils).isConnected();
        if (notify) {
            observer.onConnectionTypeChanged(0);
        }
    }

    private void disconnect(OfflinePageTabObserver observer, boolean notify) {
        doReturn(false).when(mOfflinePageUtils).isConnected();
        if (notify) {
            observer.onConnectionTypeChanged(0);
        }
    }

    @Test
    @Feature({"OfflinePages"})
    public void testBasicState() {
        OfflinePageTabObserver observer = createObserver();
        assertFalse(observer.isObservingNetworkChanges());
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartObservingTab() {
        OfflinePageTabObserver observer = createObserver();

        doReturn(false).when(mOfflinePageUtils).isOfflinePage(any(Tab.class));
        observer.startObservingTab(mTab);

        assertFalse(OfflinePageTabData.isShowingOfflinePage(mTab));
        assertFalse(OfflinePageTabData.isShowingTrustedOfflinePage(mTab));

        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(mTab));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));

        doReturn(true).when(mOfflinePageUtils).isOfflinePage(any(Tab.class));
        observer.startObservingTab(mTab);

        assertTrue(OfflinePageTabData.isShowingOfflinePage(mTab));

        assertTrue(observer.isObservingNetworkChanges());
        assertTrue(observer.isObservingTab(mTab));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStopObservingTab() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);

        // Try to stop observing a tab that is not observed.
        doReturn(42).when(mTab).getId();
        observer.stopObservingTab(mTab);

        assertFalse(observer.isObservingTab(mTab));
        doReturn(TAB_ID).when(mTab).getId();

        observer.stopObservingTab(mTab);
        assertFalse(observer.isObservingNetworkChanges());
        assertFalse(observer.isObservingTab(mTab));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnPageLoadFinished() {
        OfflinePageTabObserver observer = createObserver();

        observer.startObservingTab(mTab);
        doReturn(true).when(mOfflinePageUtils).isConnected();
        observer.onPageLoadFinished(mTab, TAB_URL);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_onPageLoadFinished() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);

        observer.startObservingTab(mTab);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertFalse(observer.wasSnackbarSeen(mTab));
        observer.onPageLoadFinished(mTab, TAB_URL);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnPageLoadFinished_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);

        observer.startObservingTab(mTab);
        assertTrue(observer.isLoadedTab(mTab));

        observer.onUrlUpdated(mTab);
        assertFalse(observer.isLoadedTab(mTab));

        observer.onPageLoadFinished(mTab, TAB_URL);
        assertTrue(observer.isLoadedTab(mTab));

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnPageLoadFinished_hidden() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        assertTrue(observer.isLoadedTab(mTab));

        observer.onUrlUpdated(mTab);
        assertFalse(observer.isLoadedTab(mTab));

        observer.onPageLoadFinished(mTab, TAB_URL);
        assertTrue(observer.isLoadedTab(mTab));

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_onShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertFalse(observer.wasSnackbarSeen(mTab));
        showTab(observer);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testDontShowPreviewSnackbar_onShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        doReturn(true).when(mOfflinePageUtils).isShowingOfflinePreview(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertFalse(observer.wasSnackbarSeen(mTab));
        showTab(observer);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnShown_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        observer.onShown(mTab, FROM_NEW);
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnShown_pageNotLoaded() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        hideTab(null);

        observer.startObservingTab(mTab);

        observer.onShown(mTab, FROM_NEW);
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden_afterSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        // Snackbar is showing over here.
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));

        hideTab(observer);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnHidden_beforeSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);

        // Snackbar is not showing over here.
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));

        hideTab(observer);

        // Snackbars if any where dismissed regardless.
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnDestroyed() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);

        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        // Snackbar was shown, so all other conditions are met.
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        observer.onDestroyed(mTab);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mTab, times(1)).removeObserver(eq(observer));
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnUrlUpdated_whenNotShowingSnackbar() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);

        // URL updated, but tab still shows offline page.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(0)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        assertTrue(OfflinePageTabData.isShowingOfflinePage(mTab));

        // URL updated and tab no longer shows offline page.
        doReturn(false).when(mOfflinePageUtils).isOfflinePage(any(Tab.class));
        observer.onUrlUpdated(mTab);

        assertFalse(OfflinePageTabData.isShowingOfflinePage(mTab));
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(2)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnUrlUpdated_whenSnackbarShown() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        // URL updated, but tab still shows offline page.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(0)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));

        observer.onPageLoadFinished(mTab, TAB_URL);

        // URL updated and tab no longer shows offline page.
        doReturn(false).when(mOfflinePageUtils).isOfflinePage(any(Tab.class));
        observer.onUrlUpdated(mTab);

        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
        verify(observer, times(1)).stopObservingTab(any(Tab.class));
        verify(mSnackbarManager, times(2)).dismissSnackbars(eq(mSnackbarController));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setObserverForTesting(mActivity, observer);

        disconnect(observer, false);
        showTab(null);

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(mTab);

        verify(mTab, times(1)).addObserver(eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserverForTab_whenConnected() {
        OfflinePageTabObserver observer = createObserver();
        OfflinePageTabObserver.setObserverForTesting(mActivity, observer);

        connect(observer, false);
        showTab(null);

        // Method under test.
        OfflinePageTabObserver.addObserverForTab(mTab);

        verify(mTab, times(1)).addObserver(eq(observer));
        verify(observer, times(1)).startObservingTab(any(Tab.class));
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_onConnectionTypeChanged() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        assertTrue(observer.isObservingNetworkChanges());

        connect(observer, true);
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_notConnected() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        assertTrue(observer.isObservingNetworkChanges());

        // Notification comes, but we are still disconnected.
        observer.onConnectionTypeChanged(0);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_tabNotShowing() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        hideTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        assertTrue(observer.isObservingNetworkChanges());
        connect(observer, true);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_pageNotLoaded() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        // That resets the page to not loaded.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        connect(observer, true);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnConnectionTypeChanged_noCurrentTab() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        hideTab(observer);
        // That resets the page to not loaded.
        observer.onUrlUpdated(mTab);

        assertTrue(observer.isObservingNetworkChanges());
        connect(observer, true);

        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingNetworkChanges());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testShowSnackbar_ignoreEventsAfterShownOnce() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        // Event ignored, snackbar not shown again.
        observer.onPageLoadFinished(mTab, TAB_URL);
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        // Event ignored, snackbar not shown again.
        observer.onShown(mTab, TabSelectionType.FROM_NEW);
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));

        // Event triggers snackbar again.
        observer.onConnectionTypeChanged(0);
        verify(observer, times(2)).showReloadSnackbar(any(Tab.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testTabRemoved_whenNotShowingSnackbar() {
        OfflinePageTabObserver observer = createObserver();

        disconnect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);

        // Snackbar is not showing over here.
        verify(observer, times(0)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));

        removeTab(observer);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        // Cannot verify using observer, because of implementation using nested class.
        verify(mTab, times(1)).removeObserver(any(OfflinePageTabObserver.class));
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testTabRemoved_whenShowingSnackbar() {
        OfflinePageTabObserver observer = createObserver();

        connect(observer, false);
        showTab(null);
        observer.startObservingTab(mTab);
        observer.onPageLoadFinished(mTab, TAB_URL);

        // Snackbar is showing over here.
        verify(observer, times(1)).showReloadSnackbar(any(Tab.class));
        assertTrue(observer.isObservingTab(mTab));
        assertTrue(observer.isLoadedTab(mTab));
        assertTrue(observer.wasSnackbarSeen(mTab));

        removeTab(observer);

        verify(mSnackbarManager, times(1)).dismissSnackbars(eq(mSnackbarController));
        // Cannot verify using observer, because of implementation using nested class.
        verify(mTab, times(1)).removeObserver(any(OfflinePageTabObserver.class));
        assertFalse(observer.isObservingTab(mTab));
        assertFalse(observer.isLoadedTab(mTab));
        assertFalse(observer.wasSnackbarSeen(mTab));
    }
}
