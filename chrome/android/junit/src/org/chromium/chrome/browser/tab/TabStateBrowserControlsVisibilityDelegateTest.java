// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

/** Tests for {@link TabStateBrowserControlsVisibilityDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
public class TabStateBrowserControlsVisibilityDelegateTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabImpl mTabImpl;
    @Mock private WebContents mWebContents;
    @Mock private NavigationHandle mNavigationHandle1;
    @Mock private NavigationHandle mNavigationHandle2;
    @Mock private NavigationHandle mNavigationHandle3;
    @Mock private SecurityStateModel.Natives mSecurityStateModelNatives;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Before
    public void setup() {
        SecurityStateModelJni.setInstanceForTesting(mSecurityStateModelNatives);
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

    HistogramWatcher expectBrowserControlLocked() {
        return HistogramWatcher.newBuilder()
                .expectBooleanRecord("Android.BrowserControls.LockedByTabState", true)
                .allowExtraRecordsForHistogramsAbove()
                .build();
    }
}
