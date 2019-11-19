// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;

import java.util.Arrays;

/** Unit tests for PageViewObserver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
public final class PageViewObserverTest {
    private static final String STARTING_URL = "http://starting.url";
    private static final String DIFFERENT_URL = "http://different.url";
    private static final String STARTING_FQDN = "starting.url";
    private static final String DIFFERENT_FQDN = "different.url";

    @Mock
    private Activity mActivity;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mTabModel;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTab2;
    @Mock
    private EventTracker mEventTracker;
    @Mock
    private TokenTracker mTokenTracker;
    @Mock
    private SuspensionTracker mSuspensionTracker;
    @Mock
    private ChromeActivity mChromeActivity;
    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabObserver mTabObserver;
    private UserDataHost mUserDataHost;
    private UserDataHost mUserDataHostTab2;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mUserDataHost = new UserDataHost();
        mUserDataHostTab2 = new UserDataHost();

        doReturn(false).when(mTab).isIncognito();
        doReturn(null).when(mTab).getUrl();
        doReturn(mChromeActivity).when(mTab).getActivity();
        doReturn(Arrays.asList(mTabModel)).when(mTabModelSelector).getModels();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(mUserDataHostTab2).when(mTab2).getUserDataHost();
        doReturn(mChromeActivity).when(mTab2).getActivity();
        doReturn(Promise.fulfilled("1")).when(mTokenTracker).getTokenForFqdn(anyString());
    }

    @Test
    public void onUpdateUrl_currentlyNull_startReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void updateUrl_nullUrl() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, null);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
        verify(mEventTracker, times(0)).addWebsiteEvent(any());
    }

    @Test
    public void updateUrl_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        reset(mEventTracker);
        updateUrl(mTab, DIFFERENT_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void updateUrl_sameDomain_startStopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        updateUrl(mTab, STARTING_URL + "/some_other_page.html");
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void updateUrl_noPaint_doesNotReportStart() {
        PageViewObserver observer = createPageViewObserver();
        updateUrlNoPaint(mTab, STARTING_URL);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        reportPaint(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void switchTabs_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        reset(mEventTracker);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        doReturn(false).when(mTab2).isHidden();
        didSelectTab(mTab2, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void switchTabs_sameDomain_startStopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));

        doReturn(STARTING_URL).when(mTab2).getUrl();
        didSelectTab(mTab2, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void switchToHiddenTab_startNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        reset(mEventTracker);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        doReturn(true).when(mTab2).isHidden();
        didSelectTab(mTab2, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void tabHidden_stopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
    }

    @Test
    public void tabShown_startReported() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab).getUrl();
        onShown(mTab, TabSelectionType.FROM_USER);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabClosed_switchToNew_startStopReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);

        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        onShown(mTab2, TabSelectionType.FROM_CLOSE);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void tabAdded_startReported() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab2).getUrl();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        didAddTab(mTab2, TabLaunchType.FROM_EXTERNAL_APP);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabAdded_notSelected_startNotReported() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab).getUrl();
        doReturn(null).when(mTabModelSelector).getCurrentTab();
        didAddTab(mTab, TabLaunchType.FROM_EXTERNAL_APP);

        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    @Test
    public void tabAdded_suspendedDomain() {
        PageViewObserver observer = createPageViewObserver();
        doReturn(STARTING_URL).when(mTab2).getUrl();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        didAddTab(mTab2, TabLaunchType.FROM_EXTERNAL_APP);

        assertEquals(SuspendedTab.from(mTab2).getFqdn(), STARTING_FQDN);
    }

    @Test
    public void tabClosed_inBackground_stopNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);
        getTabModelObserver().willCloseTab(mTab2, true);
        getTabModelObserver().tabRemoved(mTab2);

        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStopEvent(DIFFERENT_FQDN)));
    }

    // TODO(pnoland): add test for platform reporting once the System API is available in Q.

    @Test
    public void tabIncognito_eventsNotReported() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        doReturn(true).when(mTab2).isIncognito();
        doReturn(DIFFERENT_URL).when(mTab2).getUrl();
        didSelectTab(mTab2, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStopEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void navigationToSuspendedDomain_suspendedTabShown() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        doReturn(DIFFERENT_URL).when(mTab).getUrl();
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab);
        assertEquals(suspendedTab.getFqdn(), DIFFERENT_FQDN);
    }

    @Test
    public void navigationToUnsuspendedDomain_suspendedTabRemoved() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        doReturn(DIFFERENT_URL).when(mTab).getUrl();
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab);
        assertTrue(suspendedTab.isShowing());

        updateUrl(mTab, STARTING_URL);
        assertFalse(suspendedTab.isShowing());
    }

    @Test
    public void eagerSuspension() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
        assertTrue(SuspendedTab.from(mTab).isShowing());

        // Trying to suspend the site again shouldn't have an effect.
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
        assertTrue(SuspendedTab.from(mTab).isShowing());
    }

    @Test
    public void eagerSuspension_navigateToDifferentSuspended() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab);
        assertEquals(STARTING_FQDN, suspendedTab.getFqdn());

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL);

        assertEquals(DIFFERENT_FQDN, suspendedTab.getFqdn());
    }

    @Test
    public void eagerSuspension_reshowSameDomain_nowUnsuspended() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab);
        assertTrue(suspendedTab.isShowing());

        doReturn(false).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        onShown(mTab, TabSelectionType.FROM_USER);
        assertFalse(suspendedTab.isShowing());
    }

    @Test
    public void eagerUnsuspension() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab);
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
        updateUrl(mTab, STARTING_URL);

        doReturn(STARTING_URL).when(mTab).getUrl();
        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);

        SuspendedTab suspendedTab = SuspendedTab.from(mTab);
        assertTrue(suspendedTab.isShowing());

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        updateUrl(mTab, DIFFERENT_URL);

        // Notifying that STARTING_FQDN is no longer suspended shouldn't remove the active
        // SuspendedTab for DIFFERENT_FQDN.
        observer.notifySiteSuspensionChanged(STARTING_FQDN, false);
        assertTrue(suspendedTab.isShowing());
    }

    @Test
    public void eagerUnsuspension_notAlreadySuspended() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        observer.notifySiteSuspensionChanged(STARTING_FQDN, false);
        assertFalse(SuspendedTab.from(mTab).isShowing());
    }

    @Test
    public void alreadySuspendedDomain_doesNotReportStopEventAgain() {
        PageViewObserver observer = createPageViewObserver();
        updateUrl(mTab, STARTING_URL);

        observer.notifySiteSuspensionChanged(STARTING_FQDN, true);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStopEvent(STARTING_FQDN)));

        updateUrl(mTab, DIFFERENT_URL);
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
        didAddTab(mTab2, TabLaunchType.FROM_EXTERNAL_APP);
        verify(mEventTracker, times(0)).addWebsiteEvent(argThat(isStartEvent(DIFFERENT_FQDN)));
    }

    @Test
    public void construction_nullInitialTab() {
        doReturn(null).when(mTabModelSelector).getCurrentTab();
        PageViewObserver observer = createPageViewObserver();

        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
        doReturn(STARTING_URL).when(mTab).getUrl();
        didSelectTab(mTab, TabSelectionType.FROM_USER);
        verify(mEventTracker, times(1)).addWebsiteEvent(argThat(isStartEvent(STARTING_FQDN)));
    }

    private PageViewObserver createPageViewObserver() {
        PageViewObserver observer = new PageViewObserver(
                mActivity, mTabModelSelector, mEventTracker, mTokenTracker, mSuspensionTracker);
        verify(mTabModel, times(1)).addObserver(mTabModelObserverCaptor.capture());
        if (mTabModelSelector.getCurrentTab() != null) {
            verify(mTabModelSelector.getCurrentTab(), times(1))
                    .addObserver(mTabObserverCaptor.capture());
        }
        return observer;
    }

    private void updateUrl(Tab tab, String url) {
        updateUrlNoPaint(tab, url);
        reportPaint(tab, url);
    }

    private void updateUrlNoPaint(Tab tab, String url) {
        getTabObserver().onUpdateUrl(tab, url);
    }

    private void reportPaint(Tab tab, String url) {
        doReturn(url).when(tab).getUrl();
        getTabObserver().didFirstVisuallyNonEmptyPaint(tab);
    }

    private void onHidden(Tab tab, @TabHidingType int hidingType) {
        getTabObserver().onHidden(tab, hidingType);
    }

    private void onShown(Tab tab, @TabSelectionType int selectionType) {
        getTabObserver().onShown(tab, selectionType);
    }

    private void didSelectTab(Tab tab, @TabSelectionType int selectionType) {
        getTabModelObserver().didSelectTab(tab, selectionType, 0);
    }

    private void didAddTab(Tab tab, @TabLaunchType int launchType) {
        getTabModelObserver().didAddTab(tab, launchType);
    }

    private TabObserver getTabObserver() {
        if (mTabObserver == null) {
            mTabObserver = mTabObserverCaptor.getValue();
        }

        return mTabObserver;
    }

    private TabModelObserver getTabModelObserver() {
        return mTabModelObserverCaptor.getValue();
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
