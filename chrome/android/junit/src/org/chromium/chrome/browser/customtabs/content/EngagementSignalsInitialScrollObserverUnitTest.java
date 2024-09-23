// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link EngagementSignalsInitialScrollObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EngagementSignalsInitialScrollObserverUnitTest {
    private static final int SCROLL_EXTENT = 100;

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private GestureListenerManagerImpl mGestureListenerManagerImpl;

    private EngagementSignalsInitialScrollObserver mInitialScrollObserver;

    @Before
    public void setUp() {
        GestureListenerManagerImpl.setInstanceForTesting(mGestureListenerManagerImpl);
    }

    @After
    public void tearDown() {
        GestureListenerManagerImpl.setInstanceForTesting(null);
    }

    @Test
    public void testHadScrollDown() {
        initializeTabForTest();
        var gestureStateListener = captureGestureStateListener();

        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        assertTrue(mInitialScrollObserver.hasCurrentPageHadScrollDown());
    }

    @Test
    public void testHasNotHadScrollDown() {
        initializeTabForTest();
        var gestureStateListener = captureGestureStateListener();

        assertFalse(mInitialScrollObserver.hasCurrentPageHadScrollDown());
        // A scroll up shouldn't change the result.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, true);
        assertFalse(mInitialScrollObserver.hasCurrentPageHadScrollDown());
    }

    @Test
    public void testResetsOnNavigation() {
        initializeTabForTest();
        var gestureStateListener = captureGestureStateListener();
        var webContentsObserver = captureWebContentsObserver();

        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        assertTrue(mInitialScrollObserver.hasCurrentPageHadScrollDown());

        var details =
                new LoadCommittedDetails(
                        0,
                        JUnitTestGURLs.URL_1,
                        false,
                        /* isSameDocument= */ false,
                        /* isMainFrame= */ true,
                        200);
        webContentsObserver.navigationEntryCommitted(details);
        assertFalse(mInitialScrollObserver.hasCurrentPageHadScrollDown());
    }

    @Test
    public void testResetOnTabChange() {
        initializeTabForTest();
        var gestureStateListener = captureGestureStateListener();

        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        assertTrue(mInitialScrollObserver.hasCurrentPageHadScrollDown());

        // Change tabs.
        mInitialScrollObserver.onHidden(env.tabProvider.getTab(), TabHidingType.CHANGED_TABS);
        assertFalse(mInitialScrollObserver.hasCurrentPageHadScrollDown());
    }

    private void initializeTabForTest() {
        Tab initialTab = env.prepareTab();
        doAnswer(
                        invocation -> {
                            CustomTabTabObserver observer = invocation.getArgument(0);
                            initialTab.addObserver(observer);
                            observer.onAttachedToInitialTab(initialTab);
                            return null;
                        })
                .when(env.tabObserverRegistrar)
                .registerActivityTabObserver(any());

        mInitialScrollObserver =
                new EngagementSignalsInitialScrollObserver(env.tabObserverRegistrar);
        verify(env.tabObserverRegistrar).registerActivityTabObserver(mInitialScrollObserver);

        env.tabProvider.setInitialTab(initialTab, TabCreationMode.DEFAULT);
    }

    private GestureStateListener captureGestureStateListener() {
        ArgumentCaptor<GestureStateListener> gestureStateListenerArgumentCaptor =
                ArgumentCaptor.forClass(GestureStateListener.class);
        verify(mGestureListenerManagerImpl, atLeastOnce())
                .addListener(
                        gestureStateListenerArgumentCaptor.capture(),
                        eq(org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.NONE));
        return gestureStateListenerArgumentCaptor.getValue();
    }

    private WebContentsObserver captureWebContentsObserver() {
        ArgumentCaptor<WebContentsObserver> webContentsObserverArgumentCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        WebContents webContents = env.tabProvider.getTab().getWebContents();
        verify(webContents).addObserver(webContentsObserverArgumentCaptor.capture());
        return webContentsObserverArgumentCaptor.getValue();
    }
}
