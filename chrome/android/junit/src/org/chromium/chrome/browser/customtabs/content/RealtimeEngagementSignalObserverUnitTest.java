// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.ON_SCROLL_END;
import static org.chromium.chrome.browser.customtabs.content.RealtimeEngagementSignalObserver.DEFAULT_AFTER_SCROLL_END_THRESHOLD_MS;

import android.os.Bundle;
import android.os.SystemClock;

import androidx.browser.customtabs.EngagementSignalsCallback;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency;
import org.chromium.chrome.browser.customtabs.content.RealtimeEngagementSignalObserver.ScrollState;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit test for {@link RealtimeEngagementSignalObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
public class RealtimeEngagementSignalObserverUnitTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int SCROLL_EXTENT = 100;
    private static final long CURRENT_TIME_MS = 9000000L;

    RealtimeEngagementSignalObserver mEngagementSignalObserver;

    @Mock private GestureListenerManagerImpl mGestureListenerManagerImpl;
    @Mock private RenderCoordinatesImpl mRenderCoordinatesImpl;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManagerImpl;
    @Mock private TabInteractionRecorder mTabInteractionRecorder;
    @Mock private EngagementSignalsCallback mEngagementSignalsCallback;

    @Before
    public void setUp() {
        when(mRenderCoordinatesImpl.getMaxVerticalScrollPixInt()).thenReturn(SCROLL_EXTENT);
        GestureListenerManagerImpl.setInstanceForTesting(mGestureListenerManagerImpl);
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinatesImpl);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerImpl);
        TabInteractionRecorder.setInstanceForTesting(mTabInteractionRecorder);
        RealtimeEngagementSignalObserver.ScrollState.setInstanceForTesting(new ScrollState());
        doReturn(true).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS);
    }

    @After
    public void tearDown() {
        RealtimeEngagementSignalObserver.ScrollState.setInstanceForTesting(null);
        FeatureList.setTestValues(null);
    }

    @Test
    public void doesNotAddListenersForSignalsIfUmaUploadIsDisabled() {
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        initializeTabForTest();

        verify(mGestureListenerManagerImpl, never()).addListener(any(GestureStateListener.class));
    }

    @Test
    public void addsListenersForSignalsIfFeatureIsEnabled_alternativeImpl() {
        initializeTabForTest();

        verify(mGestureListenerManagerImpl)
                .addListener(any(GestureStateListener.class), eq(ON_SCROLL_END));
    }

    @Test
    public void removesGestureStateListenerWhenWebContentsWillSwap() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.webContentsWillSwap(env.tabProvider.getTab());
        }
        verify(mGestureListenerManagerImpl).removeListener(listener);
    }

    @Test
    public void removesGestureStateListenerWhenTabDetached() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onActivityAttachmentChanged(env.tabProvider.getTab(), null);
        }

        verify(env.tabProvider.getTab().getWebContents()).removeObserver(webContentsObserver);
        verify(mGestureListenerManagerImpl).removeListener(listener);
    }

    @Test
    public void reAttachGestureStateListenerWhenTabClosed() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onClosingStateChanged(env.tabProvider.getTab(), /* isClosing= */ true);
        }

        verify(env.tabProvider.getTab().getWebContents()).removeObserver(webContentsObserver);
        verify(mGestureListenerManagerImpl).removeListener(listener);
    }

    @Test
    public void reAttachGestureStateListenerWhenTabChanged() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();
        List<TabObserver> tabObservers = captureTabObservers();

        boolean hasCustomTabTabObserver = false;
        Tab anotherTab = env.prepareTab();
        for (TabObserver observer : tabObservers) {
            if (observer instanceof CustomTabTabObserver) {
                hasCustomTabTabObserver = true;
                ((CustomTabTabObserver) observer).onObservingDifferentTab(anotherTab);
            }
        }
        verify(mGestureListenerManagerImpl).removeListener(listener);
        assertTrue(
                "At least one CustomTabTabObserver should be captured.", hasCustomTabTabObserver);

        // Now, verify a new listener is attached.
        GestureStateListener listener2 = captureGestureStateListener();
        Assert.assertNotEquals(
                "A new listener should be created once tab swapped.", listener, listener2);
        verifyNoMemoryLeakForGestureStateListener(listener2);
    }

    @Test
    public void sendsSignalsForScrollStartThenEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        verify(mEngagementSignalsCallback).onVerticalScrollEvent(eq(false), any(Bundle.class));
        // End scrolling at 50%.
        listener.onScrollEnded(50, SCROLL_EXTENT);
        // We shouldn't make any more calls.
        verify(mEngagementSignalsCallback, times(1))
                .onVerticalScrollEvent(anyBoolean(), any(Bundle.class));
    }

    @Test
    public void sendsSignalsForScrollStartDirectionChangeThenEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        verify(mEngagementSignalsCallback).onVerticalScrollEvent(eq(false), any(Bundle.class));
        // Change direction to up at 10%.
        listener.onVerticalScrollDirectionChanged(true, .1f);
        verify(mEngagementSignalsCallback).onVerticalScrollEvent(eq(true), any(Bundle.class));
        // Change direction to down at 5%.
        listener.onVerticalScrollDirectionChanged(false, .05f);
        verify(mEngagementSignalsCallback, times(2))
                .onVerticalScrollEvent(eq(false), any(Bundle.class));
        // End scrolling at 50%.
        listener.onScrollEnded(50, SCROLL_EXTENT);
        // We shouldn't make any more calls.
        verify(mEngagementSignalsCallback, times(3))
                .onVerticalScrollEvent(anyBoolean(), any(Bundle.class));
    }

    @Test
    public void doesNotSendMaxScrollSignalForZeroPercent() {
        initializeTabForTest();

        // We shouldn't make any calls.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void onlySendsMaxScrollSignalAfterScrollEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 55%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(55);
        listener.onScrollOffsetOrExtentChanged(55, SCROLL_EXTENT);
        // Scroll up to 30%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(30);
        listener.onScrollOffsetOrExtentChanged(30, SCROLL_EXTENT);

        // We shouldn't make any calls at this point.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));

        // End scrolling.
        listener.onScrollEnded(30, SCROLL_EXTENT);
        // Now we should make the call.
        verify(mEngagementSignalsCallback, times(1))
                .onGreatestScrollPercentageIncreased(eq(55), any(Bundle.class));
    }

    @Test
    public void onlySendsMaxScrollSignalForFivesMultiples() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 3%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(3);
        listener.onScrollOffsetOrExtentChanged(3, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(3, SCROLL_EXTENT);
        // We shouldn't make any calls at this point.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));

        // Start scrolling down again.
        listener.onScrollStarted(3, SCROLL_EXTENT, false);
        // Scroll down to 8%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(8);
        listener.onScrollOffsetOrExtentChanged(8, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(8, SCROLL_EXTENT);
        // We should make a call for 5%.
        verify(mEngagementSignalsCallback, times(1))
                .onGreatestScrollPercentageIncreased(eq(5), any(Bundle.class));

        // Start scrolling down again.
        listener.onScrollStarted(8, SCROLL_EXTENT, false);
        // Scroll down to 94%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(94);
        listener.onScrollOffsetOrExtentChanged(94, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(94, SCROLL_EXTENT);
        // We should make a call for 90%.
        verify(mEngagementSignalsCallback, times(1))
                .onGreatestScrollPercentageIncreased(eq(90), any(Bundle.class));
    }

    @Test
    public void doesNotSendSignalForLowerPercentage() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 63%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(63);
        listener.onScrollOffsetOrExtentChanged(63, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(63, SCROLL_EXTENT);
        // We should make a call for 60%.
        verify(mEngagementSignalsCallback, times(1))
                .onGreatestScrollPercentageIncreased(eq(60), any(Bundle.class));
        clearInvocations(mEngagementSignalsCallback);

        // Now scroll back up.
        listener.onScrollStarted(63, SCROLL_EXTENT, true);
        // Scroll up to 30%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(30);
        listener.onScrollOffsetOrExtentChanged(30, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(30, SCROLL_EXTENT);

        // We shouldn't make any more calls since the max didn't change.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void doesNotSendSignalEqualToPreviousMax() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 50%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(50);
        listener.onScrollOffsetOrExtentChanged(50, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(50, SCROLL_EXTENT);

        // Now scroll up, then back down to 50%.
        listener.onScrollStarted(50, SCROLL_EXTENT, true);
        // Scroll up to 30%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(30);
        listener.onScrollOffsetOrExtentChanged(30, SCROLL_EXTENT);
        // Back down to 50%.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(50);
        listener.onScrollOffsetOrExtentChanged(50, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(50, SCROLL_EXTENT);

        // There should be only one call.
        verify(mEngagementSignalsCallback, times(1))
                .onGreatestScrollPercentageIncreased(eq(50), any(Bundle.class));
    }

    @Test
    public void resetsMaxOnNavigation_MainFrame_NewDocument() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();

        // Scroll down to 50%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(50);
        gestureStateListener.onScrollOffsetOrExtentChanged(50, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(50, SCROLL_EXTENT);

        // Verify 50% is reported.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(50), any(Bundle.class));
        clearInvocations(mEngagementSignalsCallback);

        LoadCommittedDetails details =
                new LoadCommittedDetails(
                        0,
                        JUnitTestGURLs.URL_1,
                        false,
                        /* isSameDocument= */ false,
                        /* isMainFrame= */ true,
                        200);
        webContentsObserver.navigationEntryCommitted(details);

        // Scroll down to 10%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(10);
        gestureStateListener.onScrollOffsetOrExtentChanged(10, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(10, SCROLL_EXTENT);

        // Verify 10% is reported.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(10), any(Bundle.class));
    }

    @Test
    public void doesNotResetMaxOnNavigation_MainFrame_SameDocument() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();

        // Scroll down to 30%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(30);
        gestureStateListener.onScrollOffsetOrExtentChanged(30, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(30, SCROLL_EXTENT);

        // Verify 30% is reported.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(30), any(Bundle.class));
        clearInvocations(mEngagementSignalsCallback);

        LoadCommittedDetails details =
                new LoadCommittedDetails(
                        0,
                        JUnitTestGURLs.URL_1,
                        false,
                        /* isSameDocument= */ true,
                        /* isMainFrame= */ true,
                        200);
        webContentsObserver.navigationEntryCommitted(details);

        // Scroll down to 10%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(10);
        gestureStateListener.onScrollOffsetOrExtentChanged(10, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(10, SCROLL_EXTENT);

        // Verify % isn't reported.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void doesNotResetMaxOnNavigation_SubFrame_NewDocument() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();

        // Scroll down to 90%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(90);
        gestureStateListener.onScrollOffsetOrExtentChanged(90, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(90, SCROLL_EXTENT);

        // Verify 90% is reported.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(90), any(Bundle.class));
        clearInvocations(mEngagementSignalsCallback);

        LoadCommittedDetails details =
                new LoadCommittedDetails(
                        0,
                        JUnitTestGURLs.URL_1,
                        false,
                        /* isSameDocument= */ false,
                        /* isMainFrame= */ false,
                        200);
        webContentsObserver.navigationEntryCommitted(details);

        // Scroll down to 50%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(50);
        gestureStateListener.onScrollOffsetOrExtentChanged(50, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(50, SCROLL_EXTENT);

        // Verify % isn't reported.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void resetsMaxOnTabChange() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();

        // Scroll down to 50%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(50);
        gestureStateListener.onScrollOffsetOrExtentChanged(50, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(50, SCROLL_EXTENT);

        // Verify 50% is reported.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(50), any(Bundle.class));
        clearInvocations(mEngagementSignalsCallback);

        // Change tabs.
        mEngagementSignalObserver.onHidden(env.tabProvider.getTab(), TabHidingType.CHANGED_TABS);

        // Scroll down to 10%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(10);
        gestureStateListener.onScrollOffsetOrExtentChanged(10, SCROLL_EXTENT);
        gestureStateListener.onScrollEnded(10, SCROLL_EXTENT);

        // Verify 10% is reported.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(10), any(Bundle.class));
    }

    @Test
    public void sendsSignalWithAlternativeImpl_updateBeforeEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 24%
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(24);
        listener.onScrollOffsetOrExtentChanged(24, SCROLL_EXTENT);
        // End scrolling.
        listener.onScrollEnded(24, SCROLL_EXTENT);
        // We should make a call with 20.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(20), any(Bundle.class));
    }

    @Test
    public void sendsSignalWithAlternativeImpl_updateAfterEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // End scrolling.
        listener.onScrollEnded(0, SCROLL_EXTENT);
        // Send the signal for 24% 10ms after the scroll ended.
        advanceTime(10);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(24);
        listener.onScrollOffsetOrExtentChanged(24, SCROLL_EXTENT);
        // We should make a call with 20.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(20), any(Bundle.class));
        // Any update after this will be ignored.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(25);
        listener.onScrollOffsetOrExtentChanged(25, SCROLL_EXTENT);
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(eq(25), any(Bundle.class));
    }

    @Test
    public void doesNotSendLowerPercentWithAlternativeImpl() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);

        // Start by scrolling down.
        listener.onScrollStarted(24, SCROLL_EXTENT, false);
        // End scrolling.
        listener.onScrollEnded(55, SCROLL_EXTENT);
        // Send the signal for 55% 10ms after the scroll ended.
        advanceTime(15);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(55);
        listener.onScrollOffsetOrExtentChanged(55, SCROLL_EXTENT);
        // We should make a call with 55.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(55), any(Bundle.class));

        // Scroll back up to 20%.
        listener.onScrollStarted(55, SCROLL_EXTENT, true);
        listener.onScrollEnded(20, SCROLL_EXTENT);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(20);
        listener.onScrollOffsetOrExtentChanged(20, SCROLL_EXTENT);
        // We shouldn't make any other calls (after the one from above).
        verify(mEngagementSignalsCallback, times(1))
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void doNotSendSignalWithAlternativeImplAfterThreshold() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);

        // Start by scrolling down.
        listener.onScrollStarted(59, SCROLL_EXTENT, false);
        // End scrolling.
        listener.onScrollEnded(59, SCROLL_EXTENT);
        // Send the signal for 59% 18ms outside the threshold.
        advanceTime(DEFAULT_AFTER_SCROLL_END_THRESHOLD_MS + 18);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(59);
        listener.onScrollOffsetOrExtentChanged(59, SCROLL_EXTENT);
        // We shouldn't make a call since the call was outside the threshold.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void doNotSendSignalWithAlternativeImplIfScrollStartReceived() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);

        // Start by scrolling down.
        listener.onScrollStarted(30, SCROLL_EXTENT, false);
        // End scrolling.
        listener.onScrollEnded(30, SCROLL_EXTENT);
        // Start scrolling again after 10ms
        advanceTime(10);
        listener.onScrollStarted(30, SCROLL_EXTENT, false);
        // Send update after 5ms
        advanceTime(5);
        listener.onScrollOffsetOrExtentChanged(50, SCROLL_EXTENT);
        // We shouldn't make a call since the call came after a new scroll started.
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void onAllTabsClosed_hadInteraction_sendsOnSessionEnded() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        mEngagementSignalObserver.webContentsWillSwap(tab);
        // Close all tabs.
        mEngagementSignalObserver.onClosingStateChanged(tab, true);
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        mEngagementSignalObserver.onClosingStateChanged(env.tabProvider.getTab(), true);
        mEngagementSignalObserver.onAllTabsClosed();

        verify(mEngagementSignalsCallback, times(1)).onSessionEnded(eq(true), any(Bundle.class));
    }

    @Test
    public void onAllTabsClosed_hadInteractionButIncognito_sendsOnSessionEnded() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        // Turn on Incognito.
        doReturn(true).when(tab).isIncognito();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        // User interacted.
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        mEngagementSignalObserver.webContentsWillSwap(tab);
        // Close all tabs.
        mEngagementSignalObserver.onClosingStateChanged(tab, true);
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        mEngagementSignalObserver.onClosingStateChanged(env.tabProvider.getTab(), true);
        mEngagementSignalObserver.onAllTabsClosed();

        // didUserInteract is false, even though they did
        verify(mEngagementSignalsCallback, times(1)).onSessionEnded(eq(false), any(Bundle.class));
    }

    @Test
    public void onAllTabsClosed_hadInteractionButUmaUploadDisabled_sendsOnSessionEnded() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        // Disable UMA upload.
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        // User interacted.
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        mEngagementSignalObserver.webContentsWillSwap(tab);
        // Close all tabs.
        mEngagementSignalObserver.onClosingStateChanged(tab, true);
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        mEngagementSignalObserver.onClosingStateChanged(env.tabProvider.getTab(), true);
        mEngagementSignalObserver.onAllTabsClosed();

        // didUserInteract is false, even though they did
        verify(mEngagementSignalsCallback, times(1)).onSessionEnded(eq(false), any(Bundle.class));
    }

    @Test
    public void onAllTabsClosed_hadNoInteraction_sendsOnSessionEnded() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        mEngagementSignalObserver.webContentsWillSwap(tab);
        // Close all tabs.
        mEngagementSignalObserver.onClosingStateChanged(tab, true);
        mEngagementSignalObserver.onClosingStateChanged(env.tabProvider.getTab(), true);
        mEngagementSignalObserver.onAllTabsClosed();

        verify(mEngagementSignalsCallback, times(1)).onSessionEnded(eq(false), any(Bundle.class));
    }

    @Test
    public void onAllTabsClosed_suspended_doesNotSendOnSessionEnded() {
        initializeTabForTest();
        mEngagementSignalObserver.suppressNextSessionEndedCall();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        mEngagementSignalObserver.webContentsWillSwap(tab);
        // Close all tabs.
        mEngagementSignalObserver.onClosingStateChanged(tab, true);
        mEngagementSignalObserver.onClosingStateChanged(env.tabProvider.getTab(), true);
        mEngagementSignalObserver.onAllTabsClosed();

        verify(mEngagementSignalsCallback, never()).onSessionEnded(eq(false), any(Bundle.class));

        // We should only suspend for one call.
        assertFalse(mEngagementSignalObserver.getSuspendSessionEndedForTesting());
    }

    @Test
    public void onDestroyed_hadInteraction_sendsOnSessionEnded() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        // User interacted.
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        // Tab destroyed.
        mEngagementSignalObserver.onDestroyed(tab);

        // didUserInteract is true.
        verify(mEngagementSignalsCallback, times(1)).onSessionEnded(eq(true), any(Bundle.class));
    }

    @Test
    public void onDestroyed_hadInteractionButIncognito_sendsOnSessionEnded() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        // Turn on Incognito.
        doReturn(true).when(tab).isIncognito();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        // User interacted.
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        // Tab destroyed.
        mEngagementSignalObserver.onDestroyed(tab);

        // didUserInteract is false, but they did.
        verify(mEngagementSignalsCallback, times(1)).onSessionEnded(eq(false), any(Bundle.class));
    }

    @Test
    public void onDestroyed_hadInteractionButUmaUploadDisabled_sendsOnSessionEnded() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        // Disable UMA upload.
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        // User interacted.
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        // Tab destroyed.
        mEngagementSignalObserver.onDestroyed(tab);

        // didUserInteract is false, but they did.
        verify(mEngagementSignalsCallback, times(1)).onSessionEnded(eq(false), any(Bundle.class));
    }

    @Test
    public void onDestroyed_suspended_doesNotSendOnSessionEnded() {
        initializeTabForTest();
        // Suspend.
        mEngagementSignalObserver.suppressNextSessionEndedCall();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        Tab tab = mock(Tab.class);
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        mEngagementSignalObserver.onObservingDifferentTab(tab);
        // Tab destroyed.
        mEngagementSignalObserver.onDestroyed(tab);

        // onSessionEnded not fired.
        verify(mEngagementSignalsCallback, never()).onSessionEnded(eq(false), any(Bundle.class));

        // We should only suspend for one call.
        assertFalse(mEngagementSignalObserver.getSuspendSessionEndedForTesting());
    }

    @Test
    public void pauseAndUnpauseSignalsOnPageWithTextFragment() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);
        WebContentsObserver webContentsObserver = captureWebContentsObserver();

        // Navigate to a URL with text fragment.
        var navigationHandle =
                NavigationHandle.createForTesting(
                        JUnitTestGURLs.TEXT_FRAGMENT_URL, false, 0, false);
        webContentsObserver.didStartNavigationInPrimaryMainFrame(navigationHandle);

        // Do a scroll.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(24);
        listener.onScrollOffsetOrExtentChanged(24, SCROLL_EXTENT);
        listener.onScrollEnded(24, SCROLL_EXTENT);
        // We shouldn't get scroll signals.
        verify(mEngagementSignalsCallback, never())
                .onVerticalScrollEvent(anyBoolean(), any(Bundle.class));
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));

        // Navigate back to a URL with no text fragment.
        var navigationHandle2 =
                NavigationHandle.createForTesting(JUnitTestGURLs.HTTP_URL, false, 0, false);
        webContentsObserver.didStartNavigationInPrimaryMainFrame(navigationHandle2);

        // Do a scroll.
        listener.onScrollStarted(24, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(50);
        listener.onScrollOffsetOrExtentChanged(50, SCROLL_EXTENT);
        listener.onScrollEnded(50, SCROLL_EXTENT);
        // We should normally get signals.
        verify(mEngagementSignalsCallback).onVerticalScrollEvent(eq(false), any(Bundle.class));
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(50), any(Bundle.class));
    }

    @Test
    public void doesNotSendSignalsBeforeDownScroll() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);

        // Assume we started further down on the page and scroll up.
        listener.onScrollStarted(50, SCROLL_EXTENT, true);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(30);
        listener.onScrollOffsetOrExtentChanged(30, SCROLL_EXTENT);
        listener.onScrollEnded(30, SCROLL_EXTENT);
        // We shouldn't get any signals.
        verify(mEngagementSignalsCallback, never())
                .onVerticalScrollEvent(anyBoolean(), any(Bundle.class));
        verify(mEngagementSignalsCallback, never())
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
        // Now scroll down from here.
        listener.onScrollStarted(30, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(45);
        listener.onScrollOffsetOrExtentChanged(45, SCROLL_EXTENT);
        listener.onScrollEnded(45, SCROLL_EXTENT);
        // We should get signals as if we've only scrolled down to this %.
        verify(mEngagementSignalsCallback).onVerticalScrollEvent(eq(false), any(Bundle.class));
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(45), any(Bundle.class));
    }

    @Test
    public void doesNotSendSignalsBeforeDownScroll_AfterNavigation() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener(ON_SCROLL_END);

        // Scroll down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(25);
        listener.onScrollOffsetOrExtentChanged(25, SCROLL_EXTENT);
        listener.onScrollEnded(25, SCROLL_EXTENT);
        // We should get signals as usual.
        verify(mEngagementSignalsCallback).onVerticalScrollEvent(eq(false), any(Bundle.class));
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(25), any(Bundle.class));
        // Now, navigate to another page.
        WebContentsObserver webContentsObserver = captureWebContentsObserver();
        LoadCommittedDetails details =
                new LoadCommittedDetails(
                        0,
                        JUnitTestGURLs.URL_1,
                        false,
                        /* isSameDocument= */ false,
                        /* isMainFrame= */ true,
                        200);
        webContentsObserver.navigationEntryCommitted(details);
        // Scroll up from some point in the page, e.g. back navigation or anchor fragment on page.
        // We shouldn't get any (more) signals.
        verify(mEngagementSignalsCallback, times(1))
                .onVerticalScrollEvent(anyBoolean(), any(Bundle.class));
        verify(mEngagementSignalsCallback, times(1))
                .onGreatestScrollPercentageIncreased(anyInt(), any(Bundle.class));
    }

    @Test
    public void sendInitialOffsetUpdate_AltImplEnabled() {
        initializeTabForTest(/* hadScrollDown= */ true);
        // When the alternative impl flag is enabled, the listener should be added with
        // `ON_SCROLL_END`.
        var listener = captureGestureStateListener(ON_SCROLL_END);

        // Simulate renderer sending the offset update.
        when(mRenderCoordinatesImpl.getScrollYPixInt()).thenReturn(35);
        listener.onScrollOffsetOrExtentChanged(35, SCROLL_EXTENT);

        // We should get a notification since we initialized the observer class with true for
        // hadScrollDown.
        verify(mEngagementSignalsCallback)
                .onGreatestScrollPercentageIncreased(eq(35), any(Bundle.class));
    }

    private void advanceTime(long millis) {
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS + millis);
    }

    private void initializeTabForTest(boolean hadScrollDown) {
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

        mEngagementSignalObserver =
                new RealtimeEngagementSignalObserver(
                        env.tabObserverRegistrar,
                        env.connection,
                        env.session,
                        mEngagementSignalsCallback,
                        hadScrollDown);
        verify(env.tabObserverRegistrar).registerActivityTabObserver(mEngagementSignalObserver);

        env.tabProvider.setInitialTab(initialTab, TabCreationMode.DEFAULT);
    }

    private void initializeTabForTest() {
        initializeTabForTest(false);
    }

    private GestureStateListener captureGestureStateListener() {
        return captureGestureStateListener(ON_SCROLL_END);
    }

    private GestureStateListener captureGestureStateListener(
            @RootScrollOffsetUpdateFrequency.EnumType int frequency) {
        ArgumentCaptor<GestureStateListener> gestureStateListenerArgumentCaptor =
                ArgumentCaptor.forClass(GestureStateListener.class);
        verify(mGestureListenerManagerImpl, atLeastOnce())
                .addListener(gestureStateListenerArgumentCaptor.capture(), eq(frequency));
        return gestureStateListenerArgumentCaptor.getValue();
    }

    private WebContentsObserver captureWebContentsObserver() {
        ArgumentCaptor<WebContentsObserver> webContentsObserverArgumentCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        WebContents webContents = env.tabProvider.getTab().getWebContents();
        verify(webContents).addObserver(webContentsObserverArgumentCaptor.capture());
        return webContentsObserverArgumentCaptor.getValue();
    }

    private List<TabObserver> captureTabObservers() {
        ArgumentCaptor<TabObserver> tabObserverArgumentCaptor =
                ArgumentCaptor.forClass(TabObserver.class);
        verify(env.tabProvider.getTab(), atLeastOnce())
                .addObserver(tabObserverArgumentCaptor.capture());
        return tabObserverArgumentCaptor.getAllValues();
    }

    private void verifyNoMemoryLeakForGestureStateListener(GestureStateListener listener) {
        listener.onScrollStarted(0, 1, false);
        listener.onVerticalScrollDirectionChanged(false, 0.1f);
        listener.onScrollEnded(1, 0);
    }
}
