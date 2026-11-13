// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
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

import org.chromium.base.DeviceInfo;
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
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
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
import org.chromium.content_public.browser.test.util.NavigationHandleTestBuilder;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.Origin;

import java.util.concurrent.TimeUnit;

/** Tests for {@link TabStateBrowserControlsVisibilityDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
@DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_URL)
public class TabStateBrowserControlsVisibilityDelegateTest {
    private static final String SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED =
            "Same-origin Omnibox minimization is not implemented yet, crbug.com/578150286";
    private static final int HIDDEN_TOP_CONTROLS_OFFSET = -56;
    private static final int FALLBACK_SHOW_AFTER_MS = 1500;
    private static final String MINIMIZATION_WITH_FALLBACK_PARAM =
            ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION
                    + ":fallback_show_after_ms/"
                    + FALLBACK_SHOW_AFTER_MS;

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
        when(mBrowserControlsStateProvider.getControlsPosition()).thenReturn(ControlsPosition.TOP);
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
    public void testLockedByTabState_TabContentDangerous() {
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

    private void hideTab(@TabHidingType int type) {
        mIsTabHidden = true;
        for (TabObserver observer : mTab.getTabObservers()) {
            observer.onHidden(mTab, type);
        }
    }

    private void updateSecurityLevel(@ConnectionSecurityLevel int securityLevel) {
        doReturn(securityLevel)
                .when(mSecurityStateModelNatives)
                .getSecurityLevelForWebContents(mWebContents);
        for (TabObserver observer : mTab.getTabObservers()) {
            observer.onSSLStateUpdated(mTab);
        }
    }

    private NavigationHandle startNavigation(GURL targetUrl) {
        return startNavigation(targetUrl, /* navigationId= */ 0L);
    }

    private NavigationHandle startNavigation(GURL targetUrl, long navigationId) {
        // A link click in the main frame of a page that has the same origin as targetUrl.
        NavigationHandle navigation =
                withInitiatorOrigin(
                        rendererInitiated(targetUrl, PageTransition.LINK)
                                .setNavigationId(navigationId)
                                .build(),
                        Origin.create(targetUrl));
        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(navigation);
        return navigation;
    }

    /** Returns a builder for a browser-initiated navigation with a user gesture. */
    private static NavigationHandleTestBuilder browserInitiated(
            GURL targetUrl, @PageTransition int transition) {
        return new NavigationHandleTestBuilder(targetUrl)
                .setIsRendererInitiated(false)
                .setTransition(transition)
                .setHasUserGesture(true);
    }

    /** Returns a builder for a renderer-initiated navigation with a user gesture. */
    private static NavigationHandleTestBuilder rendererInitiated(
            GURL targetUrl, @PageTransition int transition) {
        return new NavigationHandleTestBuilder(targetUrl)
                .setIsRendererInitiated(true)
                .setTransition(transition)
                .setHasUserGesture(true);
    }

    /** Returns a spy of the navigation with an initiator origin. The builder cannot set it. */
    private static NavigationHandle withInitiatorOrigin(
            NavigationHandle navigation, Origin initiatorOrigin) {
        NavigationHandle spyNavigation = spy(navigation);
        doReturn(initiatorOrigin).when(spyNavigation).getInitiatorOrigin();
        return spyNavigation;
    }

    private NavigationHandle startHistoryNavigation(
            GURL targetUrl, boolean isBack, long navigationId) {
        NavigationHandle navigation =
                browserInitiated(targetUrl, PageTransition.LINK | PageTransition.FORWARD_BACK)
                        .setIsBack(isBack)
                        .setIsForward(!isBack)
                        .setNavigationId(navigationId)
                        .build();
        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(navigation);
        return navigation;
    }

    private NavigationHandle startReloadNavigation(GURL targetUrl, long navigationId) {
        NavigationHandle navigation =
                browserInitiated(targetUrl, PageTransition.RELOAD)
                        .setIsReload(true)
                        .setNavigationId(navigationId)
                        .build();
        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(navigation);
        return navigation;
    }

    private void redirectNavigation(NavigationHandle navigation, GURL redirectUrl) {
        navigation.callDidRedirectForTesting(redirectUrl);
        mWebContentsObserver.didRedirectNavigation(navigation);
    }

    private void finishNavigation(
            NavigationHandle navigation, GURL committedUrl, boolean isSameOrigin) {
        finishNavigation(
                navigation,
                committedUrl,
                /* hasCommitted= */ true,
                isSameOrigin,
                /* isErrorPage= */ false);
    }

    private void finishNavigation(
            NavigationHandle navigation,
            GURL committedUrl,
            boolean hasCommitted,
            boolean isSameOrigin,
            boolean isErrorPage) {
        if (hasCommitted) {
            when(mWebContents.getVisibleUrl()).thenReturn(committedUrl);
            when(mWebContents.getLastCommittedUrl()).thenReturn(committedUrl);
        }
        navigation.callDidFinishForTesting(committedUrl, hasCommitted, isSameOrigin, isErrorPage);
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

    private void verifyControlsUnlockedPushed() {
        verify(mConstraintsHelperJniMock)
                .updateState(
                        same(mWebContents),
                        eq(BrowserControlsState.BOTH),
                        eq(BrowserControlsState.BOTH),
                        eq(true),
                        any());
    }

    private void verifyControlsShownWithoutAnimation() {
        verify(mConstraintsHelperJniMock)
                .updateState(
                        same(mWebContents),
                        eq(BrowserControlsState.SHOWN),
                        eq(BrowserControlsState.SHOWN),
                        /* animate= */ eq(false),
                        any());
        verify(mConstraintsHelperJniMock, never())
                .updateState(
                        any(),
                        eq(BrowserControlsState.SHOWN),
                        anyInt(),
                        /* animate= */ eq(true),
                        any());
    }

    private void verifyControlsNeverLockedShown() {
        verify(mConstraintsHelperJniMock, never())
                .updateState(any(), eq(BrowserControlsState.SHOWN), anyInt(), anyBoolean(), any());
    }

    private void assertVisibilityConstraints(
            TabStateBrowserControlsVisibilityDelegate delegate,
            @BrowserControlsState int expected) {
        assertEquals(expected, delegate.calculateVisibilityConstraints());
        assertEquals(expected, TabBrowserControlsConstraintsHelper.getConstraints(mTab));
    }

    private void assertPostLoadLockExpiresAfterDelay(
            TabStateBrowserControlsVisibilityDelegate delegate) {
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        long loadDelayMs = TabStateBrowserControlsVisibilityDelegate.getLoadDelayMs();
        ShadowSystemClock.advanceBy(loadDelayMs - 1, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        ShadowSystemClock.advanceBy(1, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertVisibilityConstraints(delegate, BrowserControlsState.BOTH);
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_SameOriginNavigationKeepsControlsHidden() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        // No renderer push on navigation start.
        verify(mConstraintsHelperJniMock, never())
                .updateState(any(), anyInt(), anyInt(), anyBoolean(), any());

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);

        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        // The new page already starts hidden, so the commit push only syncs BOTH constraints.
        verifyControlsUnlockedPushed();

        // The slow-load fallback does not fire after the commit.
        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_SameOriginBackNavigationKeepsControlsHidden() {
        assertSameOriginHistoryNavigationKeepsControlsHidden(/* isBack= */ true);
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_SameOriginForwardNavigationKeepsControlsHidden() {
        assertSameOriginHistoryNavigationKeepsControlsHidden(/* isBack= */ false);
    }

    private void assertSameOriginHistoryNavigationKeepsControlsHidden(boolean isBack) {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        // History navigations are browser-initiated but still eligible, so no lock on start.
        NavigationHandle navigation =
                startHistoryNavigation(destinationUrl, isBack, /* navigationId= */ 1L);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verify(mConstraintsHelperJniMock, never())
                .updateState(any(), anyInt(), anyInt(), anyBoolean(), any());

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);

        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        // The new page already starts hidden, so the commit push only syncs BOTH constraints.
        verifyControlsUnlockedPushed();

        // The slow-load fallback does not fire after the commit.
        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_VisibleSameOriginLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        // Controls are visible (ratio 0.0f) rather than fully off-screen.
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredDelegate(sourceUrl, /* hiddenRatio= */ 0.0f, /* topControlsOffset= */ 0);

        NavigationHandle navigation = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_CrossOriginLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL crossOriginUrl = JUnitTestGURLs.RED_1;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(crossOriginUrl);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        finishNavigation(navigation, crossOriginUrl, /* isSameOrigin= */ false);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_CrossOriginRedirectLocksAtCommit() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        GURL crossOriginRedirectUrl = JUnitTestGURLs.RED_1;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        // A redirect displays no content, so the origin is checked only at commit.
        redirectNavigation(navigation, crossOriginRedirectUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();

        finishNavigation(navigation, crossOriginRedirectUrl, /* isSameOrigin= */ false);

        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        // The cross-origin page is already on screen, so its URL must not slide in.
        verifyControlsShownWithoutAnimation();

        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_RedirectBackToSameOriginStaysHidden() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        GURL crossOriginRedirectUrl = JUnitTestGURLs.RED_1;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);

        // A -> B -> A redirect chain (e.g. an SSO check); B never displays content.
        redirectNavigation(navigation, crossOriginRedirectUrl);
        redirectNavigation(navigation, destinationUrl);

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);

        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_SameOriginCommitsErrorPage() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        // Keep isSameOrigin() true to isolate the isErrorPage() check.
        finishNavigation(
                navigation,
                destinationUrl,
                /* hasCommitted= */ true,
                /* isSameOrigin= */ true,
                /* isErrorPage= */ true);

        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsShownWithoutAnimation();

        // Error pages hold LockReason.TAB_ERROR even after the 3s post-load lock expires.
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_NonCommittedFinishStaysHidden() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);

        // Navigation finishes without committing (e.g. download or 204).
        finishNavigation(
                navigation,
                destinationUrl,
                /* hasCommitted= */ false,
                /* isSameOrigin= */ false,
                /* isErrorPage= */ false);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        // The slow-load fallback does not fire for a finished navigation.
        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_UserRevealsDuringSlowLoadThenCommits() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        // User scrolls up to reveal the omnibox while the load is still in flight.
        when(mBrowserControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(0.25f);
        TabBrowserControlsOffsetHelper.get(mTab)
                .setOffsets(
                        /* topControlsOffset= */ -14,
                        /* contentOffset= */ 42,
                        /* topControlsMinHeightOffset= */ 0,
                        /* bottomControlsOffset= */ 0,
                        /* bottomControlsMinHeightOffset= */ 0);

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);

        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsShownWithoutAnimation();

        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_SlowLoadFallbackLocksBeforeCommit() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS - 1, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();

        // Reach fallback_show_after_ms while the navigation is still in flight.
        ShadowSystemClock.advanceBy(1, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        // Time spent waiting after fallback fires does not eat into the post-commit lock window.
        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_SlowLoadFallbackCountsFromFirstOverlappingNavigation() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL firstDestinationUrl = JUnitTestGURLs.BLUE_2;
        GURL secondDestinationUrl = JUnitTestGURLs.BLUE_3;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle firstNav = startNavigation(firstDestinationUrl, /* navigationId= */ 1L);
        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS - 100, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        NavigationHandle secondNav = startNavigation(secondDestinationUrl, /* navigationId= */ 2L);
        finishNavigation(
                firstNav,
                firstDestinationUrl,
                /* hasCommitted= */ false,
                /* isSameOrigin= */ false,
                /* isErrorPage= */ false);
        ShadowSystemClock.advanceBy(99, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();

        // The user has waited fallback_show_after_ms since navigation 1 started, and aborting
        // navigation 1 did not cancel the timer while navigation 2 remained in flight.
        ShadowSystemClock.advanceBy(1, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());

        finishNavigation(secondNav, secondDestinationUrl, /* isSameOrigin= */ true);
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_AbortedNavigationCancelsSlowLoadFallback() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        finishNavigation(
                navigation,
                destinationUrl,
                /* hasCommitted= */ false,
                /* isSameOrigin= */ false,
                /* isErrorPage= */ false);

        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_TabHiddenMidNavigationLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        // Hide and re-show the tab while the navigation is still in flight. Clear the pushes from
        // the TAB_HIDDEN lock so the check below only sees the push made on return.
        hideTab(TabHidingType.ACTIVITY_HIDDEN);
        clearInvocations(mConstraintsHelperJniMock);
        mTab.show(TabSelectionType.FROM_USER);

        // On return the in-flight navigation holds the load lock until commit, even past
        // getLoadDelayMs().
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        long loadDelayMs = TabStateBrowserControlsVisibilityDelegate.getLoadDelayMs();
        ShadowSystemClock.advanceBy(loadDelayMs, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_SlowLoadFallbackRestartsAfterReplacedNavigation() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL firstDestinationUrl = JUnitTestGURLs.BLUE_2;
        GURL secondDestinationUrl = JUnitTestGURLs.BLUE_3;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        // Content usually finishes a replaced navigation before the new one starts. No navigation
        // that skipped the load lock is in flight in between, so the fallback counts from the
        // start of navigation 2.
        NavigationHandle firstNav = startNavigation(firstDestinationUrl, /* navigationId= */ 1L);
        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS - 100, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        finishNavigation(
                firstNav,
                firstDestinationUrl,
                /* hasCommitted= */ false,
                /* isSameOrigin= */ false,
                /* isErrorPage= */ false);
        startNavigation(secondDestinationUrl, /* navigationId= */ 2L);

        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS - 1, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();

        ShadowSystemClock.advanceBy(1, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION + ":fallback_show_after_ms/0")
    public void testSameOriginMinimization_ZeroFallbackLocksAtStart() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // A delay of 0 or less shows the Omnibox at once, so the navigation takes the lock.
        assertSameOriginNavigationLocksControls(delegate);
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_TabDestroyedCancelsSlowLoadFallback() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        // The navigation skips the load lock and starts the slow-load fallback.
        startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        // Destroying the tab cancels the fallback, so the controls never lock.
        mTab.destroy();
        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @DisabledTest(message = "crbug.com/579544435")
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_EditableFocusAtNavigationStartLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        // Focus an editable node before navigation starts, then blur it before commit so only the
        // navigation load lock keeps the controls SHOWN.
        delegate.onNodeAttributeUpdated(/* editable= */ true, /* password= */ false);
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        NavigationHandle navigation = startNavigation(destinationUrl);
        delegate.onNodeAttributeUpdated(/* editable= */ false, /* password= */ false);
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @DisabledTest(message = "crbug.com/579544435")
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_DangerousContentAtNavigationStartLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        updateSecurityLevel(ConnectionSecurityLevel.DANGEROUS);
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        NavigationHandle navigation = startNavigation(destinationUrl);
        updateSecurityLevel(ConnectionSecurityLevel.NONE);
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @DisabledTest(message = "crbug.com/579544435")
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_ErrorPageAtNavigationStartLocksControls() {
        GURL initialUrl = JUnitTestGURLs.RED_1;
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(initialUrl);

        // Commit an error page on sourceUrl and let its load lock expire so only TAB_ERROR holds
        // the controls SHOWN when the next navigation starts.
        NavigationHandle errorNavigation = startNavigation(sourceUrl, /* navigationId= */ 1L);
        finishNavigation(
                errorNavigation,
                sourceUrl,
                /* hasCommitted= */ true,
                /* isSameOrigin= */ false,
                /* isErrorPage= */ true);
        ShadowSystemClock.advanceBy(
                TabStateBrowserControlsVisibilityDelegate.getLoadDelayMs(), TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mTab.isShowingErrorPage());
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        NavigationHandle navigation = startNavigation(destinationUrl, /* navigationId= */ 2L);

        // Committing destinationUrl clears mTab.isShowingErrorPage() via TabWebContentsObserver,
        // leaving only the navigation load lock to hold SHOWN for getLoadDelayMs().
        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertFalse(mTab.isShowingErrorPage());
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @DisabledTest(message = "crbug.com/579544435")
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_AccessibilityEnabledAtNavigationStartLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        AccessibilityStateTestHelper.setAccessibilityEnabledForTesting(true);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());

        NavigationHandle navigation = startNavigation(destinationUrl);
        AccessibilityStateTestHelper.setAccessibilityEnabledForTesting(false);
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_EditableFocusAfterHiddenCommitLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertVisibilityConstraints(delegate, BrowserControlsState.BOTH);

        delegate.onNodeAttributeUpdated(/* editable= */ true, /* password= */ true);
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        delegate.onNodeAttributeUpdated(/* editable= */ false, /* password= */ false);
        assertVisibilityConstraints(delegate, BrowserControlsState.BOTH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_DangerousContentAfterHiddenCommitLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle navigation = startNavigation(destinationUrl);
        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertVisibilityConstraints(delegate, BrowserControlsState.BOTH);

        updateSecurityLevel(ConnectionSecurityLevel.DANGEROUS);
        assertVisibilityConstraints(delegate, BrowserControlsState.SHOWN);

        updateSecurityLevel(ConnectionSecurityLevel.NONE);
        assertVisibilityConstraints(delegate, BrowserControlsState.BOTH);
    }

    @Test
    @DisabledTest(message = SAME_ORIGIN_MINIMIZATION_NOT_IMPLEMENTED)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_OverlappingEligibleNavigationsCheckedAtEachCommit() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL firstDestinationUrl = JUnitTestGURLs.BLUE_2;
        GURL secondDestinationUrl = JUnitTestGURLs.BLUE_3;
        GURL crossOriginUrl = JUnitTestGURLs.RED_1;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle firstNav = startNavigation(firstDestinationUrl, /* navigationId= */ 1L);
        NavigationHandle secondNav = startNavigation(secondDestinationUrl, /* navigationId= */ 2L);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        finishNavigation(firstNav, firstDestinationUrl, /* isSameOrigin= */ true);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());

        // Navigation 2 redirected and commits on another origin after navigation 1 committed.
        redirectNavigation(secondNav, crossOriginUrl);
        finishNavigation(secondNav, crossOriginUrl, /* isSameOrigin= */ false);

        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsShownWithoutAnimation();

        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_FlagDisabledParityWithBaseline() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        // Start navigation: locks to SHOWN and pushes (SHOWN, BOTH, true).
        NavigationHandle navigation = startNavigation(destinationUrl);
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
        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
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
        verifyControlsUnlockedPushed();
    }

    private void assertSameOriginNavigationLocksControls(
            TabStateBrowserControlsVisibilityDelegate delegate) {
        assertStartedNavigationLocksControls(delegate, startNavigation(JUnitTestGURLs.BLUE_2));
    }

    /** Starts the navigation and checks that it takes the load lock and keeps it. */
    private void assertNavigationLocksControls(
            TabStateBrowserControlsVisibilityDelegate delegate, NavigationHandle navigation) {
        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(navigation);
        assertStartedNavigationLocksControls(delegate, navigation);
    }

    /** Checks that a started navigation takes the load lock and keeps it after it finishes. */
    private void assertStartedNavigationLocksControls(
            TabStateBrowserControlsVisibilityDelegate delegate, NavigationHandle navigation) {
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        finishNavigation(navigation, navigation.getUrl(), /* isSameOrigin= */ true);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_OutsideTabbedBrowserNeverTriggers() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // Custom Tabs, TWAs, and WebAPKs.
        mTab.setIsTabInBrowser(false);
        assertSameOriginNavigationLocksControls(delegate);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_ReadsControlsOfTabsCurrentWindow() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        NavigationHandle firstNav = startNavigation(destinationUrl);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        finishNavigation(firstNav, destinationUrl, /* isSameOrigin= */ true);

        // The tab moves to another window, whose controls are fully visible.
        when(mOtherBrowserControlsStateProvider.getBrowserControlHiddenRatio()).thenReturn(0.0f);
        mOtherProviderSupplier =
                attachProvider(mOtherWindowAndroid, mOtherBrowserControlsStateProvider);
        mTab.updateAttachment(mOtherWindowAndroid, /* tabDelegateFactory= */ null);

        NavigationHandle secondNav = startNavigation(sourceUrl);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
        finishNavigation(secondNav, sourceUrl, /* isSameOrigin= */ true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_BottomControlsNeverTriggers() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        when(mBrowserControlsStateProvider.getControlsPosition())
                .thenReturn(ControlsPosition.BOTTOM);
        assertSameOriginNavigationLocksControls(delegate);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_AutomotiveNeverTriggers() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertSameOriginNavigationLocksControls(delegate);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_TvNeverTriggers() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        DeviceInfo.setIsTVForTesting(true);
        assertSameOriginNavigationLocksControls(delegate);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_XrNeverTriggers() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        DeviceInfo.setIsXrForTesting(true);
        assertSameOriginNavigationLocksControls(delegate);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_SubdomainChangeTreatedAsCrossOrigin() {
        GURL httpsRootUrl = new GURL("https://example.com/page1");
        GURL httpsSubdomainUrl = new GURL("https://sub.example.com/page2");

        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(httpsRootUrl);

        NavigationHandle subdomainNav = startNavigation(httpsSubdomainUrl);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
        finishNavigation(subdomainNav, httpsSubdomainUrl, /* isSameOrigin= */ false);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_FromFilesystemUrlLocksControls() {
        // GURL#getOrigin() of a filesystem: URL is its inner https origin.
        GURL filesystemUrl = new GURL("filesystem:https://www.blue.com/temporary/page1");
        GURL httpsUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(filesystemUrl);

        NavigationHandle navigation = startNavigation(httpsUrl);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
        finishNavigation(navigation, httpsUrl, /* isSameOrigin= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_SchemeChangeTreatedAsCrossOrigin() {
        GURL httpUrl = new GURL("http://example.com/page1");
        GURL httpsUrl = new GURL("https://example.com/page1");

        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(httpUrl);

        NavigationHandle schemeNav = startNavigation(httpsUrl);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();
        finishNavigation(schemeNav, httpsUrl, /* isSameOrigin= */ false);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_BrowserInitiatedSameOriginLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // A URL typed in the Omnibox.
        assertNavigationLocksControls(
                delegate, browserInitiated(JUnitTestGURLs.BLUE_2, PageTransition.TYPED).build());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_RendererInitiatedSameOriginReloadKeepsControlsHidden() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);
        // A location.reload() call from the page. A cross-origin frame cannot reload the top
        // frame, so a same-origin initiator means that the page started it.
        NavigationHandle navigation =
                withInitiatorOrigin(
                        rendererInitiated(
                                        sourceUrl,
                                        PageTransition.LINK | PageTransition.CLIENT_REDIRECT)
                                .setHasUserGesture(false)
                                .setIsReload(true)
                                .build(),
                        Origin.create(sourceUrl));

        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(navigation);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verify(mConstraintsHelperJniMock, never())
                .updateState(any(), anyInt(), anyInt(), anyBoolean(), any());

        finishNavigation(navigation, sourceUrl, /* isSameOrigin= */ true);
        assertEquals(BrowserControlsState.BOTH, delegate.calculateVisibilityConstraints());
        verifyControlsNeverLockedShown();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_RestoredHistoryNavigationLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // A back navigation to a restored entry that has not loaded yet. Content also sets
        // isHistory() for it, but it must lock.
        NavigationHandle navigation =
                spy(
                        browserInitiated(
                                        JUnitTestGURLs.BLUE_2,
                                        PageTransition.LINK | PageTransition.FORWARD_BACK)
                                .setIsBack(true)
                                .build());
        doReturn(true).when(navigation).isRestore();
        assertNavigationLocksControls(delegate, navigation);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_CrossOriginInitiatorLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // A cross-origin iframe navigates the top frame to the origin of the page.
        assertNavigationLocksControls(
                delegate,
                withInitiatorOrigin(
                        rendererInitiated(JUnitTestGURLs.BLUE_2, PageTransition.LINK).build(),
                        Origin.create(JUnitTestGURLs.RED_1)));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_NullInitiatorLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // A renderer-initiated load without an initiator, for example an intent that Chrome
        // routes to itself. The builder sets no initiator. No FROM_API, so only the null check
        // locks it.
        assertNavigationLocksControls(
                delegate, rendererInitiated(JUnitTestGURLs.BLUE_2, PageTransition.LINK).build());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_OpaqueInitiatorLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // A Tab Group Sync update or a web sign-in redirect loads the URL with an opaque initiator.
        assertNavigationLocksControls(
                delegate,
                withInitiatorOrigin(
                        rendererInitiated(JUnitTestGURLs.BLUE_2, PageTransition.LINK)
                                .setHasUserGesture(false)
                                .build(),
                        Origin.createOpaqueOrigin()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_SelfRoutedIntentLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // An intent that Chrome routes to itself copies isRendererInitiated from the intent.
        // Production self-routed intents have no initiator. This test gives a same-origin
        // initiator to isolate the FROM_API check.
        assertNavigationLocksControls(
                delegate,
                withInitiatorOrigin(
                        rendererInitiated(
                                        JUnitTestGURLs.BLUE_2,
                                        PageTransition.LINK | PageTransition.FROM_API)
                                .build(),
                        Origin.create(JUnitTestGURLs.BLUE_1)));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_RendererInitiatedHistoryNavigationLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        // A history.back() call. The frame that called it can be a cross-origin iframe.
        assertNavigationLocksControls(
                delegate,
                withInitiatorOrigin(
                        rendererInitiated(
                                        JUnitTestGURLs.BLUE_2,
                                        PageTransition.LINK | PageTransition.FORWARD_BACK)
                                .setIsBack(true)
                                .build(),
                        Origin.create(JUnitTestGURLs.BLUE_1)));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_ReloadLocksControls() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        assertStartedNavigationLocksControls(
                delegate, startReloadNavigation(JUnitTestGURLs.BLUE_1, /* navigationId= */ 1L));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_CrossOriginHistoryNavigationLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL crossOriginUrl = JUnitTestGURLs.RED_1;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);
        // A browser-initiated history pending entry updates getVisibleUrl() before navigation
        // start while getLastCommittedUrl() still reflects sourceUrl.
        when(mWebContents.getVisibleUrl()).thenReturn(crossOriginUrl);

        NavigationHandle navigation =
                startHistoryNavigation(crossOriginUrl, /* isBack= */ true, /* navigationId= */ 1L);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        finishNavigation(navigation, crossOriginUrl, /* isSameOrigin= */ false);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(MINIMIZATION_WITH_FALLBACK_PARAM)
    public void testSameOriginMinimization_EligibleNavigationKeepsLockOfSupersededNavigation() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        // Navigation 1 is a keyboard reload: same-origin but browser-initiated, so not eligible.
        NavigationHandle navigation1 = startReloadNavigation(sourceUrl, /* navigationId= */ 1L);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        // Navigation 2 is an eligible same-origin navigation, e.g. location.href, that supersedes
        // navigation 1, so navigation 1 finishes without committing.
        NavigationHandle navigation2 = startNavigation(destinationUrl, /* navigationId= */ 2L);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        finishNavigation(
                navigation1,
                sourceUrl,
                /* hasCommitted= */ false,
                /* isSameOrigin= */ false,
                /* isErrorPage= */ false);

        // Stay inside the slow-load fallback window so navigation 2 commits before it fires.
        ShadowSystemClock.advanceBy(FALLBACK_SHOW_AFTER_MS / 2, TimeUnit.MILLISECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());

        // Navigation 2 keeps the lock acquired by navigation 1 until the post-load delay, which
        // starts at navigation 2's finish, expires.
        finishNavigation(navigation2, destinationUrl, /* isSameOrigin= */ true);
        assertPostLoadLockExpiresAfterDelay(delegate);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testSameOriginMinimization_BackgroundTabNavigationLocksControls() {
        GURL sourceUrl = JUnitTestGURLs.BLUE_1;
        GURL destinationUrl = JUnitTestGURLs.BLUE_2;
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(sourceUrl);

        // The window's controls offsets follow the foreground tab, so they say nothing about a
        // background tab. Its navigation takes the lock.
        hideTab(TabHidingType.CHANGED_TABS);
        NavigationHandle navigation = startNavigation(destinationUrl);

        clearInvocations(mConstraintsHelperJniMock);
        mTab.show(TabSelectionType.FROM_USER);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
        verifyControlsLockedShown();

        finishNavigation(navigation, destinationUrl, /* isSameOrigin= */ true);
        assertEquals(BrowserControlsState.SHOWN, delegate.calculateVisibilityConstraints());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION + ":arm/minimize_omnibox")
    public void testSameOriginMinimization_MinimizeOmniboxArmNeverTriggers() {
        TabStateBrowserControlsVisibilityDelegate delegate =
                createWiredHiddenControlsDelegate(JUnitTestGURLs.BLUE_1);
        assertSameOriginNavigationLocksControls(delegate);
    }

    HistogramWatcher expectBrowserControlLocked() {
        return HistogramWatcher.newBuilder()
                .expectBooleanRecord("Android.BrowserControls.LockedByTabState", true)
                .allowExtraRecordsForHistogramsAbove()
                .build();
    }
}
