// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabViewManager;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;

/** Unit tests for PageViewObserver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class PageViewObserverTest {
    private static final GURL STARTING_URL = JUnitTestGURLs.URL_1;
    private static final GURL STARTING_URL_WITH_PATH = JUnitTestGURLs.URL_1_WITH_PATH;
    private static final GURL DIFFERENT_URL = JUnitTestGURLs.URL_2;
    private static final String STARTING_FQDN = "www.one.com";
    private static final String DIFFERENT_FQDN = "www.two.com";

    @Mock private Activity mActivity;
    @Mock private ObservableSupplier<Tab> mTabSupplier;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private EventTracker mEventTracker;
    @Mock private TokenTracker mTokenTracker;
    @Mock private SuspensionTracker mSuspensionTracker;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ChromeActivity mChromeActivity;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Captor private ArgumentCaptor<Callback<Tab>> mTabSupplierCaptor;

    private UserDataHost mUserDataHost;
    private UserDataHost mUserDataHostTab2;
    private UserDataHost mDestroyedUserDataHost;
    private WeakReference<Activity> mActivityRef;

    private static class MockTabViewManager implements TabViewManager {
        private TabViewProvider mTabViewProvider;

        @Override
        public boolean isShowing(TabViewProvider tabViewProvider) {
            return mTabViewProvider != null && mTabViewProvider == tabViewProvider;
        }

        @Override
        public void addTabViewProvider(TabViewProvider tabViewProvider) {
            mTabViewProvider = tabViewProvider;
        }

        @Override
        public void removeTabViewProvider(TabViewProvider tabViewProvider) {
            if (mTabViewProvider == tabViewProvider) mTabViewProvider = null;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mUserDataHost = new UserDataHost();
        mUserDataHostTab2 = new UserDataHost();
        mDestroyedUserDataHost = new UserDataHost();
        mDestroyedUserDataHost.destroy();

        Activity activity = Robolectric.buildActivity(TestActivity.class).get();

        doReturn(false).when(mTab).isIncognito();
        doReturn(null).when(mTab).getUrl();
        doReturn(activity).when(mTab).getContext();
        doReturn(activity).when(mTab2).getContext();
        doReturn(new MockTabViewManager()).when(mTab).getTabViewManager();
        doReturn(new MockTabViewManager()).when(mTab2).getTabViewManager();
        doReturn(true).when(mTab).isInitialized();
        doReturn(true).when(mTab2).isInitialized();
        doReturn(mTab).when(mTabSupplier).get();
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(mUserDataHostTab2).when(mTab2).getUserDataHost();
        doReturn(Promise.fulfilled("1")).when(mTokenTracker).getTokenForFqdn(anyString());

        mActivityRef = new WeakReference<>(mChromeActivity);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab2.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);
    }

    @Test
    public void onUpdateUrl_currentlyNull_startReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void updateUrl_nullUrl() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, null, observer);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN, observer);
        verify(mEventTracker, times(0)).addWebsiteEvent(any());
    }

    @Test
    public void updateUrl_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        reset(mEventTracker);
        updateUrl(mTab, DIFFERENT_URL, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void updateUrl_sameDomain_startStopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));

        updateUrl(mTab, STARTING_URL_WITH_PATH, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void updateUrl_noPaint_doesNotReportStart() {
        PageViewObserver observer = createPageViewObserver();
        updateUrlNoPaint(mTab, STARTING_URL, observer);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        reportPaint(mTab, STARTING_URL, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void switchTabs_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        reset(mEventTracker);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        doReturn(mTab2).when(mTabSupplier).get();
        doReturn(false).when(mTab2).isHidden();
        changeTab(mTab2);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void switchTabs_sameDomain_startStopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));

        doReturn(STARTING_URL).when(mTab2).getUrl();
        changeTab(mTab2);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void switchToHiddenTab_startNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        reset(mEventTracker);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        doReturn(true).when(mTab2).isHidden();
        changeTab(mTab2);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void switchToSuspendedTab_startNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(STARTING_URL).when(mTab).getUrl();
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
        assertTrue(SuspendedTab.from(mTab, mTabContentManagerSupplier).isShowing());
        reset(mEventTracker);

        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN, observer);
        onShown(mTab, TabSelectionType.FROM_USER, observer);
        verify(mEventTracker, never()).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabHidden_stopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN, observer);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void tabShown_startReported() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab).getUrl();
        onShown(mTab, TabSelectionType.FROM_USER, observer);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabClosed_switchToNew_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN, observer);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        onShown(mTab2, TabSelectionType.FROM_CLOSE, observer);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void tabAdded_startReported() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab2).getUrl();
        doReturn(mTab2).when(mTabSupplier).get();
        changeTab(mTab2);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabAdded_notSelected_startNotReported() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab).getUrl();
        doReturn(null).when(mTabSupplier).get();
        changeTab(mTab);

        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabAdded_suspendedDomain() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab2).getUrl();
        doReturn(mTab2).when(mTabSupplier).get();
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        changeTab(mTab2);

        assertEquals(SuspendedTab.from(mTab2, mTabContentManagerSupplier).getFqdn(), STARTING_FQDN);
    }

    // TODO(pnoland): add test for platform reporting once the System API is available in Q.

    @Test
    public void tabIncognito_eventsNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(true).when(mTab2).isIncognito();
        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        changeTab(mTab2);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStopEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void navigationToSuspendedDomain_suspendedTabShown() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(DIFFERENT_URL).when(mTab).getUrl();
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL, observer);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab, mTabContentManagerSupplier);
        assertEquals(suspendedTab.getFqdn(), DIFFERENT_FQDN);
    }

    @Test
    public void navigationToUnsuspendedDomain_suspendedTabRemoved() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(DIFFERENT_URL).when(mTab).getUrl();
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL, observer);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab, mTabContentManagerSupplier);
        assertTrue(suspendedTab.isShowing());

        updateUrl(mTab, STARTING_URL, observer);
        assertFalse(suspendedTab.isShowing());
    }

    @Test
    public void eagerSuspension() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
        assertTrue(SuspendedTab.from(mTab, mTabContentManagerSupplier).isShowing());

        // Trying to suspend the site again shouldn't have an effect.
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
        assertTrue(SuspendedTab.from(mTab, mTabContentManagerSupplier).isShowing());
    }

    @Test
    public void eagerSuspension_navigateToDifferentSuspended() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab, mTabContentManagerSupplier);
        assertEquals(STARTING_FQDN, suspendedTab.getFqdn());

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL, observer);

        assertEquals(DIFFERENT_FQDN, suspendedTab.getFqdn());
    }

    @Test
    public void eagerSuspension_reshowSameDomain_nowUnsuspended() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab, mTabContentManagerSupplier);
        assertTrue(suspendedTab.isShowing());

        doReturn(false).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        onShown(mTab, TabSelectionType.FROM_USER, observer);
        assertFalse(suspendedTab.isShowing());
    }

    @Test
    public void eagerUnsuspension() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab, mTabContentManagerSupplier);
        assertTrue(suspendedTab.isShowing());

        observer.notifySiteSuspensionChanged(STARTING_FQDN, false);
        assertFalse(suspendedTab.isShowing());

        // Trying to un-suspend again should have no effect.
        observer.notifySiteSuspensionChanged(STARTING_FQDN, false);
        assertFalse(suspendedTab.isShowing());
    }

    @Test
    public void eagerUnsuspension_otherDomainActiveAndSuspended() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab, mTabContentManagerSupplier);
        assertTrue(suspendedTab.isShowing());

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL, observer);

        // Notifying that STARTING_FQDN is no longer suspended shouldn't remove the active
        // SuspendedTab for DIFFERENT_FQDN.
        observer.notifySiteSuspensionChanged(STARTING_FQDN, false);
        assertTrue(suspendedTab.isShowing());
    }

    @Test
    public void eagerUnsuspension_notAlreadySuspended() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        observer.notifySiteSuspensionChanged(STARTING_FQDN, false);
        assertFalse(SuspendedTab.from(mTab, mTabContentManagerSupplier).isShowing());
    }

    @Test
    public void alreadySuspendedDomain_doesNotReportStopEventAgain() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));

        updateUrl(mTab, DIFFERENT_URL, observer);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void customTab_startReportedUponConstruction() {
        doReturn(STARTING_URL).when(mTab).getUrl();
        doReturn(false).when(mTab).isHidden();
        PageViewObserver observer = createPageViewObserver();
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        doReturn(true).when(mTab2).isHidden();
        changeTab(mTab2);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void construction_nullInitialTab() {
        doReturn(null).when(mTabSupplier).get();
        PageViewObserver observer = createPageViewObserver();

        doReturn(mTab).when(mTabSupplier).get();
        doReturn(STARTING_URL).when(mTab).getUrl();
        changeTab(mTab);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void eagerSuspension_destroyedTab() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        doReturn(mDestroyedUserDataHost).when(mTab).getUserDataHost();
        doReturn(false).when(mTab).isInitialized();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
    }

    @Test
    public void eagerSuspension_nullTab() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL, observer);

        changeTab(null);
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
    }

    private PageViewObserver createPageViewObserver() {
        PageViewObserver observer =
                new PageViewObserver(
                        mActivity,
                        mTabSupplier,
                        mEventTracker,
                        mTokenTracker,
                        mSuspensionTracker,
                        mTabContentManagerSupplier);
        verify(mTabSupplier, times(1)).addObserver(mTabSupplierCaptor.capture());
        Tab tab = mTabSupplier.get();
        mTabSupplierCaptor.getValue().onResult(tab);
        if (tab != null) {
            verify(tab, times(1)).addObserver(observer);
        }

        return observer;
    }

    private void updateUrl(Tab tab, GURL url, TabObserver tabObserver) {
        updateUrlNoPaint(tab, url, tabObserver);
        reportPaint(tab, url, tabObserver);
    }

    private void updateUrlNoPaint(Tab tab, GURL url, TabObserver tabObserver) {
        tabObserver.onUpdateUrl(tab, url);
    }

    private void reportPaint(Tab tab, GURL url, TabObserver tabObserver) {
        doReturn(url).when(tab).getUrl();
        tabObserver.didFirstVisuallyNonEmptyPaint(tab);
    }

    private void onHidden(Tab tab, @TabHidingType int hidingType, TabObserver tabObserver) {
        tabObserver.onHidden(tab, hidingType);
    }

    private void onShown(Tab tab, @TabSelectionType int selectionType, TabObserver tabObserver) {
        tabObserver.onShown(tab, selectionType);
    }

    private void changeTab(Tab newTab) {
        mTabSupplierCaptor.getValue().onResult(newTab);
    }

    private ArgumentMatcher<WebsiteEvent> isStartEvent(String fqdn) {
        return new ArgumentMatcher<WebsiteEvent>() {
            @Override
            public boolean matches(WebsiteEvent event) {
                return event.getType() == WebsiteEvent.EventType.START
                        && event.getFqdn().equals(fqdn);
            }

            @Override
            public String toString() {
                return "Start event with fqdn: " + fqdn;
            }
        };
    }

    private ArgumentMatcher<WebsiteEvent> isStopEvent(String fqdn) {
        return new ArgumentMatcher<WebsiteEvent>() {
            @Override
            public boolean matches(WebsiteEvent event) {
                return event.getType() == WebsiteEvent.EventType.STOP
                        && event.getFqdn().equals(fqdn);
            }

            @Override
            public String toString() {
                return "Stop event with fqdn: " + fqdn;
            }
        };
    }
}
