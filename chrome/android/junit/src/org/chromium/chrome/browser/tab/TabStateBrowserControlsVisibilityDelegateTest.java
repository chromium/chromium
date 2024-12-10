// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
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
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetError;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

/** Tests for {@link TabStateBrowserControlsVisibilityDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
@LooperMode(Mode.PAUSED)
public class TabStateBrowserControlsVisibilityDelegateTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabImpl mTabImpl;
    @Mock private WebContents mWebContents;
    @Mock private NavigationHandle mNavigationHandle1;
    @Mock private NavigationHandle mNavigationHandle2;
    @Mock private NavigationHandle mNavigationHandle3;
    @Mock private SecurityStateModel.Natives mSecurityStateModelNatives;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Before
    public void setup() {
        mJniMocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateModelNatives);
    }

    @Test
    @DisableFeatures("ControlsVisibilityFromNavigations")
    public void testOnPageLoadFailedDuringNavigation() {
        // Inspired by https://crbug.com/1447237.
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

        tabObserver.onPageLoadStarted(mTabImpl, blueGurl);
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onPageLoadStarted(mTabImpl, blueGurl);
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);
        tabObserver.onPageLoadStarted(mTabImpl, redGurl);
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle3);
        tabObserver.onPageLoadFailed(mTabImpl, NetError.ERR_ABORTED);
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);
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
        tabObserver.onPageLoadStarted(mTabImpl, JUnitTestGURLs.BLUE_1);

        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onPageLoadFinished(mTabImpl, JUnitTestGURLs.BLUE_1);

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
        tabObserver.onPageLoadStarted(mTabImpl, blueGurl);

        // Start navigation 2 before navigation 1 finishes.
        tabObserver.onDidStartNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);
        tabObserver.onPageLoadStarted(mTabImpl, redGurl);

        // Finish navigation 1.
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle1);
        tabObserver.onPageLoadFinished(mTabImpl, blueGurl);

        // Wait for 3 seconds, giving the timers a chance to run.
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();

        // Should still be locked as navigation is outstanding.
        assertEquals(
                BrowserControlsState.SHOWN,
                controlsVisibilityDelegate.calculateVisibilityConstraints());

        // Now finish navigation 2 and wait 3 seconds, should unlock.
        tabObserver.onDidFinishNavigationInPrimaryMainFrame(mTabImpl, mNavigationHandle2);
        tabObserver.onPageLoadFinished(mTabImpl, redGurl);
        ShadowSystemClock.advanceBy(3, TimeUnit.SECONDS);
        ShadowLooper.runUiThreadTasks();
        assertEquals(
                BrowserControlsState.BOTH,
                controlsVisibilityDelegate.calculateVisibilityConstraints());
    }
}
