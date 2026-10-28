// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProviderSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Tests for {@link TabStateBrowserControlsVisibilityDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
@DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_URL)
public class TabStateBrowserControlsVisibilityDelegateTest {
    private static final String SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED =
            "Same-origin Omnibox minimization is not implemented yet, crbug.com/578150286";
    private static final int HIDDEN_TOP_CONTROLS_OFFSET = -56;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabImpl mTabImpl;

    @Mock(extraInterfaces = {WebContentsObserver.Observable.class})
    private WebContents mWebContents;

    @Mock private Profile mProfile;
    @Mock private NavigationHandle mNavigationHandle1;
    @Mock private NavigationHandle mNavigationHandle2;
    @Mock private NavigationHandle mNavigationHandle3;
    @Mock private SecurityStateModel.Natives mSecurityStateModelNatives;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private BrowserControlsStateProvider mOtherBrowserControlsStateProvider;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WindowAndroid mOtherWindowAndroid;
    @Mock private TabBrowserControlsConstraintsHelper.Natives mConstraintsHelperJniMock;
    @Mock private TabDelegateFactory mTabDelegateFactory;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private MockTab mTab;
    private boolean mIsTabHidden;
    private WebContentsObserver mWebContentsObserver;

    // Held here because UnownedUserDataHost keeps only weak references to its data.
    private MonotonicObservableSupplier<BrowserControlsStateProvider> mProviderSupplier;
    private MonotonicObservableSupplier<BrowserControlsStateProvider> mOtherProviderSupplier;

    @Before
    public void setup() {
        SecurityStateModelJni.setInstanceForTesting(mSecurityStateModelNatives);
        TabBrowserControlsConstraintsHelperJni.setInstanceForTesting(mConstraintsHelperJniMock);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
    }

    private static MonotonicObservableSupplier<BrowserControlsStateProvider> attachProvider(
            WindowAndroid window, BrowserControlsStateProvider provider) {
        UnownedUserDataHost host = new UnownedUserDataHost();
        when(window.getUnownedUserDataHost()).thenReturn(host);
        when(window.getOcclusionSupplier()).thenReturn(ObservableSuppliers.alwaysFalse());
        MonotonicObservableSupplier<BrowserControlsStateProvider> supplier =
                ObservableSuppliers.createMonotonic(provider);
        BrowserControlsStateProviderSupplier.attach(host, supplier);
        return supplier;
    }

    @Test
    public void testAbortedNavigationFinishesAfterNewerNavigationStarts() {
        // Inspired by https://crbug.com/40064686. Navigation 2 is aborted after navigation 3 has
        // already started, so the lock must be held until navigation 3 finishes too.
        GURL blueGurl = JUnitTestGURLs.BLUE_1;
        GURL redGurl = JUnitTestGURLs.RED_1;
        when(mTabImpl.getUrl()).thenReturn(blueGurl);

        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle2.getNavigationId()).thenReturn(2L);
        when(mNavigationHandle2.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle3.getNavigationId()).thenReturn(3L);
        when(mNavigationHandle3.getUrl()).thenReturn(redGurl);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // Set this after constructor to dodge the ImeAdapter#fromWebContents().
        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle3);

        // Navigation 2 finishes (aborted) while navigation 3 is still in flight.
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle3);
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    public void sameDocumentNavigationsIgnored() {
        when(mTabImpl.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(mNavigationHandle1.isSameDocument()).thenReturn(true);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // Set this after constructor to dodge the ImeAdapter#fromWebContents().
        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    public void testInterleavedSlowNavigations() {
        // Inspired by https://crbug.com/379652406
        GURL blueGurl = JUnitTestGURLs.BLUE_1;
        GURL redGurl = JUnitTestGURLs.RED_1;
        when(mTabImpl.getUrl()).thenReturn(blueGurl);

        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle2.getNavigationId()).thenReturn(2L);
        when(mNavigationHandle2.getUrl()).thenReturn(redGurl);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // Set this after constructor to dodge the ImeAdapter#fromWebContents().
        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        // Start navigation 1.
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);

        // Start navigation 2 before navigation 1 finishes.
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);

        // Finish navigation 1.
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);

        // Wait for 3 seconds, giving the timers a chance to run.
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();

        // Should still be locked as navigation is outstanding.
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        // Now finish navigation 2 and wait 3 seconds, should unlock.
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.BROWSER_CONTROLS_LOCKOUT_DURATION + ":fullscreen_load_delay_ms/1500")
    public void testFullscreenLoadDelay_customDuration() {
        assertEquals(1500, TabStateBrowserControlsVisibilityDelegate.getLoadDelayMs());

        GURL blueGurl = JUnitTestGURLs.BLUE_1;
        when(mTabImpl.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle1.isSameDocument()).thenReturn(false);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);

        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(1000, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.BROWSER_CONTROLS_LOCKOUT_DURATION + ":fullscreen_load_delay_ms/500")
    public void testFullscreenLoadDelay_belowSecurityMinClamped() {
        assertEquals(1000, TabStateBrowserControlsVisibilityDelegate.getLoadDelayMs());

        GURL blueGurl = JUnitTestGURLs.BLUE_1;
        when(mTabImpl.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle1.isSameDocument()).thenReturn(false);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);

        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.BROWSER_CONTROLS_LOCKOUT_DURATION + ":fullscreen_load_delay_ms/5000")
    public void testFullscreenLoadDelay_aboveSecurityMaxClamped() {
        assertEquals(3000, TabStateBrowserControlsVisibilityDelegate.getLoadDelayMs());

        GURL blueGurl = JUnitTestGURLs.BLUE_1;
        when(mTabImpl.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(blueGurl);
        when(mNavigationHandle1.isSameDocument()).thenReturn(false);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);

        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(2500, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.BROWSER_CONTROLS_LOCKOUT_DURATION + ":fullscreen_load_delay_ms/1500")
    public void testFullscreenLoadDelay_disabledLoadingCheckReturnsZero() {
        TabStateBrowserControlsVisibilityDelegate.disablePageLoadDelayForTests();
        assertEquals(0, TabStateBrowserControlsVisibilityDelegate.getLoadDelayMs());
    }

    @Test
    public void testHiddenDuringNavigationKeepsLock() {
        // Inspired by https://crbug.com/567765567
        when(mTabImpl.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // Set this after constructor to dodge the ImeAdapter#fromWebContents().
        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        // Background and foreground the tab while the navigation is still in flight.
        tabObserver.onHidden(mTabImpl, TabHidingType.ACTIVITY_HIDDEN);
        tabObserver.onShown(mTabImpl, TabSelectionType.FROM_USER);
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();

        // Should still be locked as the navigation is outstanding.
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        // Finishing the navigation unlocks after the usual delay.
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    public void testHiddenAfterNavigationReleasesLock() {
        when(mTabImpl.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);

        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // Set this after constructor to dodge the ImeAdapter#fromWebContents().
        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        // With no navigation outstanding, hiding the tab releases the lock immediately.
        tabObserver.onHidden(mTabImpl, TabHidingType.ACTIVITY_HIDDEN);
        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }

    @Test
    public void testLockedByTabState_ChromeUrl() {
        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        doReturn(JUnitTestGURLs.NTP_URL).when(mTabImpl).getUrl();
        doReturn(mWebContents).when(mTabImpl).getWebContents();

        try (var ignored = expectBrowserControlLocked()) {
            controlsVisibilityDelegate.calculateVisibilityConstraints();
        }
    }

    @Test
    public void testLockedByTabState_TabContentDagerous() {
        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        doReturn(JUnitTestGURLs.BLUE_1).when(mTabImpl).getUrl();
        doReturn(mWebContents).when(mTabImpl).getWebContents();
        doReturn(ConnectionSecurityLevel.DANGEROUS)
                .when(mSecurityStateModelNatives)
                .getSecurityLevelForWebContents(mWebContents);

        try (var ignored = expectBrowserControlLocked()) {
            controlsVisibilityDelegate.calculateVisibilityConstraints();
        }
    }

    @Test
    public void testLockedByTabState_EditableNodeFocus() {
        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        doReturn(JUnitTestGURLs.BLUE_1).when(mTabImpl).getUrl();
        doReturn(mWebContents).when(mTabImpl).getWebContents();
        controlsVisibilityDelegate.onNodeAttributeUpdated(true, true);

        try (var ignored = expectBrowserControlLocked()) {
            controlsVisibilityDelegate.calculateVisibilityConstraints();
        }
    }

    @Test
    public void testLockedByTabState_TabError() {
        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        doReturn(JUnitTestGURLs.BLUE_1).when(mTabImpl).getUrl();
        doReturn(mWebContents).when(mTabImpl).getWebContents();
        doReturn(true).when(mTabImpl).isShowingErrorPage();

        try (var ignored = expectBrowserControlLocked()) {
            controlsVisibilityDelegate.calculateVisibilityConstraints();
        }
    }

    @Test
    public void testLockedByTabState_TabHidden() {
        TabStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate =
                new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        doReturn(JUnitTestGURLs.BLUE_1).when(mTabImpl).getUrl();
        doReturn(mWebContents).when(mTabImpl).getWebContents();
        doReturn(true).when(mTabImpl).isHidden();

        try (var ignored = expectBrowserControlLocked()) {
            controlsVisibilityDelegate.calculateVisibilityConstraints();
        }
    }

    @Test
    public void testLockedByTabState_IsLoadingFullscreen() {
        when(mTabImpl.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(mNavigationHandle1.getNavigationId()).thenReturn(1L);
        when(mNavigationHandle1.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(mNavigationHandle1.isSameDocument()).thenReturn(false);

        new TabStateBrowserControlsVisibilityDelegate(mTabImpl);
        verify(mTabImpl).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // Set this after constructor to dodge the ImeAdapter#fromWebContents().
        when(mTabImpl.getWebContents()).thenReturn(mWebContents);

        try (var ignored = expectBrowserControlLocked()) {
            tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        }
    }

    private TabStateBrowserControlsVisibilityDelegate createWiredDelegate(
            GURL initialUrl, float hiddenRatio, int topControlsOffset) {
        mProviderSupplier = attachProvider(mWindowAndroid, mBrowserControlsStateProvider);
        mIsTabHidden = false;
        mTab =
                new MockTab(1, mProfile) {
                    @Override
                    public boolean isHidden() {
                        return mIsTabHidden;
                    }

                    @Override
                    public void show(@TabSelectionType int type) {
                        mIsTabHidden = false;
                        for (TabObserver observer : mObservers) {
                            observer.onShown(this, type);
                        }
                    }

                    @Override
                    public void willShowBrowserControls() {
                        boolean hasViewTransitionOptIn = getWebContents().hasViewTransitionOptIn();
                        for (TabObserver observer : mObservers) {
                            observer.onWillShowBrowserControls(this, hasViewTransitionOptIn);
                        }
                    }
                };
        mTab.setIsTabInBrowser(true);
        when(mWebContents.getVisibleUrl()).thenReturn(initialUrl);
        when(mWebContents.getLastCommittedUrl()).thenReturn(initialUrl);
        when(mBrowserControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(hiddenRatio);

        TabHelpers.initTabHelpers(mTab, /* parentTab= */ null);
        TabBrowserControlsOffsetHelper.get(mTab)
                .setOffsets(
                        topControlsOffset,
                        /* contentOffset= */ topControlsOffset + 56,
                        /* topControlsMinHeightOffset= */ 0,
                        /* bottomControlsOffset= */ 0,
                        /* bottomControlsMinHeightOffset= */ 0);

        TabStateBrowserControlsVisibilityDelegate delegate =
                new TabStateBrowserControlsVisibilityDelegate(mTab);
        when(mTabDelegateFactory.createBrowserControlsVisibilityDelegate(mTab))
                .thenReturn(delegate);
        mTab.updateAttachment(mWindowAndroid, mTabDelegateFactory);
        mTab.setWebContentsOverrideForTesting(mWebContents);
        mTab.setIsInitialized(true);
        TabBrowserControlsConstraintsHelper.updateVisibilityDelegate(mTab);

        TabWebContentsObserver tabWebContentsObserver = TabWebContentsObserver.from(mTab);
        tabWebContentsObserver.initWebContents(mWebContents);
        mWebContentsObserver = tabWebContentsObserver.getWebContentsObserverForTesting();

        delegate.updateVisibilityConstraints();
        ShadowLooper.idleMainLooper();
        clearInvocations(mConstraintsHelperJniMock);
        return delegate;
    }

    private TabStateBrowserControlsVisibilityDelegate createWiredHiddenControlsDelegate(
            GURL initialUrl) {
        return createWiredDelegate(initialUrl, /* hiddenRatio= */ 1.0f, HIDDEN_TOP_CONTROLS_OFFSET);
    }

    private NavigationHandle startNavigation(GURL targetUrl) {
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        targetUrl,
                        /* isInPrimaryMainFrame= */ true,
                        /* isSameDocument= */ false,
                        /* isRendererInitiated= */ true,
                        PageTransition.LINK,
                        /* hasUserGesture= */ true,
                        /* isReload= */ false);
        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(navigation);
        return navigation;
    }

    private void finishNavigation(
            NavigationHandle navigation, GURL committedUrl, boolean isSameOrigin) {
        when(mWebContents.getVisibleUrl()).thenReturn(committedUrl);
        when(mWebContents.getLastCommittedUrl()).thenReturn(committedUrl);
        navigation.didFinish(
                committedUrl,
                /* isErrorPage= */ false,
                /* hasCommitted= */ true,
                /* isPrimaryMainFrameFragmentNavigation= */ false,
                /* isDownload= */ false,
                /* isValidSearchFormUrl= */ false,
                PageTransition.LINK,
                NetError.OK,
                /* errorDescription= */ "",
                /* httpStatuscode= */ 200,
                /* isExternalProtocol= */ false,
                /* isPdf= */ false,
                /* mimeType= */ "",
                /* currentPage= */ null,
                isSameOrigin,
                /* responseHeaders= */ Collections.emptyMap(),
                /* ignoredDuplicateNavigationCount= */ 0);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(navigation);
    }

    private void verifyControlsLockedShown() {
        verify(mConstraintsHelperJniMock, atLeastOnce())
                .updateState(
                        same(mWebContents),
                        eq(BrowserControlsState.SHOWN),
                        eq(BrowserControlsState.BOTH),
                        eq(true),
                        any());
    }

    private void verifyControlsNeverLockedShown() {
        verify(mConstraintsHelperJniMock, never())
                .updateState(any(), eq(BrowserControlsState.SHOWN), anyInt(), anyBoolean(), any());
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_SAME_ORIGIN_MINIMIZATION)
    public void testSameOriginMinimization_SameOriginNavigationKeepsControlsHidden() {
        GURL page1 = JUnitTestGURLs.BLUE_1;
        GURL page2 = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(page1);

        NavigationHandle navigation = startNavigation(page2);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        // No renderer push on navigation start.
        verify(mConstraintsHelperJniMock, never())
                .updateState(any(), anyInt(), anyInt(), anyBoolean(), any());

        finishNavigation(navigation, page2, /* isSameOrigin= */ true);

        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        // The new page already starts hidden, so the commit push only syncs BOTH constraints.
        verify(mConstraintsHelperJniMock)
                .updateState(
                        same(mWebContents),
                        eq(BrowserControlsState.BOTH),
                        eq(BrowserControlsState.BOTH),
                        eq(true),
                        any());

        // The slow-load fallback does not fire after the commit.
        ShadowSystemClock.advanceBy(1, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_SAME_ORIGIN_MINIMIZATION)
    public void testSameOriginMinimization_VisibleSameOriginLocksControls() {
        GURL page1 = JUnitTestGURLs.BLUE_1;
        GURL page2 = JUnitTestGURLs.BLUE_2;
        // Controls are visible (ratio 0.0f) rather than fully off-screen.
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredDelegate(page1, /* hiddenRatio= */ 0.0f, /* topControlsOffset= */ 0);

        NavigationHandle navigation = startNavigation(page2);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        finishNavigation(navigation, page2, /* isSameOrigin= */ true);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_SAME_ORIGIN_MINIMIZATION)
    public void testSameOriginMinimization_CrossOriginLocksControls() {
        GURL page1 = JUnitTestGURLs.BLUE_1;
        GURL crossOrigin = JUnitTestGURLs.RED_1;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(page1);

        NavigationHandle navigation = startNavigation(crossOrigin);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        finishNavigation(navigation, crossOrigin, /* isSameOrigin= */ false);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_OMNIBOX_SAME_ORIGIN_MINIMIZATION)
    public void testSameOriginMinimization_FlagDisabledParityWithBaseline() {
        GURL page1 = JUnitTestGURLs.BLUE_1;
        GURL page2 = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(page1);

        // Start navigation: locks to SHOWN and pushes (SHOWN, BOTH, true).
        NavigationHandle navigation = startNavigation(page2);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verify(mConstraintsHelperJniMock)
                .updateState(
                        same(mWebContents),
                        eq(BrowserControlsState.SHOWN),
                        eq(BrowserControlsState.BOTH),
                        eq(true),
                        any());
        clearInvocations(mConstraintsHelperJniMock);

        // Finish navigation: pushes (SHOWN, BOTH, true) once (no unconditional update(SHOWN,false)
        // when topControlsOffset != 0).
        finishNavigation(navigation, page2, /* isSameOrigin= */ true);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verify(mConstraintsHelperJniMock)
                .updateState(
                        same(mWebContents),
                        eq(BrowserControlsState.SHOWN),
                        eq(BrowserControlsState.BOTH),
                        eq(true),
                        any());
        clearInvocations(mConstraintsHelperJniMock);

        // After 3s load delay: unlocks to BOTH and pushes (BOTH, BOTH, true).
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verify(mConstraintsHelperJniMock)
                .updateState(
                        same(mWebContents),
                        eq(BrowserControlsState.BOTH),
                        eq(BrowserControlsState.BOTH),
                        eq(true),
                        any());
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_SAME_ORIGIN_MINIMIZATION)
    public void testSameOriginMinimization_ReadsControlsOfTabsCurrentWindow() {
        GURL page1 = JUnitTestGURLs.BLUE_1;
        GURL page2 = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(page1);

        NavigationHandle firstNav = startNavigation(page2);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        finishNavigation(firstNav, page2, /* isSameOrigin= */ true);

        // The tab moves to another window, whose controls are fully visible.
        when(mOtherBrowserControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(0.0f);
        mOtherProviderSupplier =
                attachProvider(mOtherWindowAndroid, mOtherBrowserControlsStateProvider);
        mTab.updateAttachment(mOtherWindowAndroid, /* tabDelegateFactory= */ null);

        NavigationHandle secondNav = startNavigation(page1);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
        finishNavigation(secondNav, page1, /* isSameOrigin= */ true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_SAME_ORIGIN_MINIMIZATION)
    public void testSameOriginMinimization_SubdomainChangeTreatedAsCrossOrigin() {
        GURL httpsRoot = new GURL("https://example.com/page1");
        GURL httpsSubdomain = new GURL("https://sub.example.com/page2");

        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(httpsRoot);

        NavigationHandle subdomainNav = startNavigation(httpsSubdomain);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
        finishNavigation(subdomainNav, httpsSubdomain, /* isSameOrigin= */ false);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_SAME_ORIGIN_MINIMIZATION)
    public void testSameOriginMinimization_SchemeChangeTreatedAsCrossOrigin() {
        GURL httpRoot = new GURL("http://example.com/page1");
        GURL httpsRoot = new GURL("https://example.com/page1");

        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(httpRoot);

        NavigationHandle schemeNav = startNavigation(httpsRoot);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
        finishNavigation(schemeNav, httpsRoot, /* isSameOrigin= */ false);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    HistogramWatcher expectBrowserControlLocked() {
        return HistogramWatcher.newBuilder()
                .expectBooleanRecord("Android.BrowserControls.LockedByTabState", true)
                .allowExtraRecordsForHistogramsAbove()
                .build();
    }
}
