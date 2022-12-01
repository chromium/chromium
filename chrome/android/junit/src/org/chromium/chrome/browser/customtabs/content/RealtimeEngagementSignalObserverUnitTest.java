// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.content.RealtimeEngagementSignalObserver.REAL_VALUES;
import static org.chromium.url.JUnitTestGURLs.URL_1;

import android.graphics.Point;

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

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.content.RealtimeEngagementSignalObserver.ScrollState;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit test for {@link RealtimeEngagementSignalObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
public class RealtimeEngagementSignalObserverUnitTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Rule
    public Features.JUnitProcessor processor = new Features.JUnitProcessor();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int SCROLL_EXTENT = 100;

    RealtimeEngagementSignalObserver mEngagementSignalObserver;

    @Mock
    private GestureListenerManagerImpl mGestureListenerManagerImpl;
    @Mock
    private RenderCoordinatesImpl mRenderCoordinatesImpl;
    @Mock
    private PrivacyPreferencesManagerImpl mPrivacyPreferencesManagerImpl;
    @Mock
    private TabInteractionRecorder mTabInteractionRecorder;

    @Before
    public void setUp() {
        when(mRenderCoordinatesImpl.getMaxVerticalScrollPixInt()).thenReturn(100);
        GestureListenerManagerImpl.setInstanceForTesting(mGestureListenerManagerImpl);
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinatesImpl);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerImpl);
        TabInteractionRecorder.setInstanceForTesting(mTabInteractionRecorder);
        RealtimeEngagementSignalObserver.ScrollState.setInstanceForTesting(new ScrollState());
        doReturn(true).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
    }

    @After
    public void tearDown() {
        RenderCoordinatesImpl.setInstanceForTesting(null);
        GestureListenerManagerImpl.setInstanceForTesting(null);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(null);
        TabInteractionRecorder.setInstanceForTesting(null);
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
    public void addsListenersForSignalsIfFeatureIsEnabled() {
        initializeTabForTest();

        verify(mGestureListenerManagerImpl).addListener(any(GestureStateListener.class));
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
            observer.onClosingStateChanged(env.tabProvider.getTab(), /*isClosing*/ true);
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
    public void sendUserInteractionOnTabDestroyed_NoUserInteraction() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection).notifyDidGetUserInteraction(eq(env.session), eq(false));
    }

    @Test
    public void sendUserInteractionOnTabDestroyed_DidGetUserInteraction() {
        initializeTabForTest();
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection).notifyDidGetUserInteraction(eq(env.session), eq(true));
    }

    @Test
    public void sendUserInteractionOnTabHidden() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onHidden(env.tabProvider.getTab(), TabHidingType.ACTIVITY_HIDDEN);
        }
        verify(env.connection).notifyDidGetUserInteraction(eq(env.session), eq(false));
    }

    @Test
    public void sendUserInteractionOnTabHidden_OtherReason() {
        initializeTabForTest();
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onHidden(env.tabProvider.getTab(), TabHidingType.CHANGED_TABS);
            observer.onHidden(env.tabProvider.getTab(), TabHidingType.REPARENTED);
        }
        verify(env.connection, never()).notifyDidGetUserInteraction(eq(env.session), eq(false));
    }

    @Test
    public void doesNotSendUserInteractionWhenIncognito() {
        env.isIncognito = true;
        initializeTabForTest();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection, never()).notifyDidGetUserInteraction(eq(env.session), anyBoolean());
    }

    @Test
    public void doesNotSendUserInteractionWhenUmaUploadDisabled() {
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        initializeTabForTest();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection, never()).notifyDidGetUserInteraction(eq(env.session), anyBoolean());
    }

    @Test
    public void sendsSignalsForScrollStartThenEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        verify(env.connection).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // End scrolling at 50%.
        listener.onScrollEnded(50, SCROLL_EXTENT);
        // We shouldn't make any more calls.
        verify(env.connection, times(1)).notifyVerticalScrollEvent(eq(env.session), anyBoolean());
    }

    @Test
    public void sendsSignalsForScrollStartDirectionChangeThenEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        verify(env.connection).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // Change direction to up at 10%.
        listener.onVerticalScrollDirectionChanged(true, .1f);
        verify(env.connection).notifyVerticalScrollEvent(eq(env.session), eq(true));
        // Change direction to down at 5%.
        listener.onVerticalScrollDirectionChanged(false, .05f);
        verify(env.connection, times(2)).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // End scrolling at 50%.
        listener.onScrollEnded(50, SCROLL_EXTENT);
        // We shouldn't make any more calls.
        verify(env.connection, times(3)).notifyVerticalScrollEvent(eq(env.session), anyBoolean());
    }

    @Test
    public void doesNotSendMaxScrollSignalForZeroPercent() {
        initializeTabForTest();

        // We shouldn't make any calls.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());
    }

    @Test
    public void onlySendsMaxScrollSignalAfterScrollEnd() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 55%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 55));
        // Scroll up to 30%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 30));

        // We shouldn't make any calls at this point.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());

        // End scrolling.
        listener.onScrollEnded(30, SCROLL_EXTENT);
        // Now we should make the call.
        verify(env.connection, times(1))
                .notifyGreatestScrollPercentageIncreased(eq(env.session), eq(55));
    }

    @Test
    public void onlySendsMaxScrollSignalForFivesMultiples() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 3%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 3));
        // End scrolling.
        listener.onScrollEnded(3, SCROLL_EXTENT);
        // We shouldn't make any calls at this point.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());

        // Start scrolling down again.
        listener.onScrollStarted(3, SCROLL_EXTENT, false);
        // Scroll down to 8%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 8));
        // End scrolling.
        listener.onScrollEnded(8, SCROLL_EXTENT);
        // We should make a call for 5%.
        verify(env.connection, times(1))
                .notifyGreatestScrollPercentageIncreased(eq(env.session), eq(5));

        // Start scrolling down again.
        listener.onScrollStarted(8, SCROLL_EXTENT, false);
        // Scroll down to 94%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 94));
        // End scrolling.
        listener.onScrollEnded(94, SCROLL_EXTENT);
        // We should make a call for 90%.
        verify(env.connection, times(1))
                .notifyGreatestScrollPercentageIncreased(eq(env.session), eq(90));
    }

    @Test
    public void doesNotSendSignalForLowerPercentage() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 63%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 63));
        // End scrolling.
        listener.onScrollEnded(63, SCROLL_EXTENT);
        // We should make a call for 60%.
        verify(env.connection, times(1))
                .notifyGreatestScrollPercentageIncreased(eq(env.session), eq(60));
        clearInvocations(env.connection);

        // Now scroll back up.
        listener.onScrollStarted(63, SCROLL_EXTENT, true);
        // Scroll up to 30%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 30));
        // End scrolling.
        listener.onScrollEnded(30, SCROLL_EXTENT);

        // We shouldn't make any more calls since the max didn't change.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());
    }

    @Test
    public void doesNotSendSignalEqualToPreviousMax() {
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 50%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 50));
        // End scrolling.
        listener.onScrollEnded(50, SCROLL_EXTENT);

        // Now scroll up, then back down to 50%.
        listener.onScrollStarted(50, SCROLL_EXTENT, true);
        // Scroll up to 30%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 30));
        // Back down to 50%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 50));
        // End scrolling.
        listener.onScrollEnded(50, SCROLL_EXTENT);

        // There should be only one call.
        verify(env.connection, times(1))
                .notifyGreatestScrollPercentageIncreased(eq(env.session), eq(50));
    }

    @Test
    public void resetsMaxOnNavigation_MainFrame_NewDocument() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();

        // Scroll down to 50%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 50));
        gestureStateListener.onScrollEnded(50, SCROLL_EXTENT);

        // Verify 50% is reported.
        verify(env.connection).notifyGreatestScrollPercentageIncreased(eq(env.session), eq(50));
        clearInvocations(env.connection);

        LoadCommittedDetails details = new LoadCommittedDetails(0, JUnitTestGURLs.getGURL(URL_1),
                false, /*isSameDocument=*/false, /*isMainFrame=*/true, 200);
        webContentsObserver.navigationEntryCommitted(details);

        // Scroll down to 10%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 10));
        gestureStateListener.onScrollEnded(10, SCROLL_EXTENT);

        // Verify 10% is reported.
        verify(env.connection).notifyGreatestScrollPercentageIncreased(eq(env.session), eq(10));
    }

    @Test
    public void doesNotResetMaxOnNavigation_MainFrame_SameDocument() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();

        // Scroll down to 30%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 30));
        gestureStateListener.onScrollEnded(30, SCROLL_EXTENT);

        // Verify 30% is reported.
        verify(env.connection).notifyGreatestScrollPercentageIncreased(eq(env.session), eq(30));
        clearInvocations(env.connection);

        LoadCommittedDetails details = new LoadCommittedDetails(0, JUnitTestGURLs.getGURL(URL_1),
                false, /*isSameDocument=*/true, /*isMainFrame=*/true, 200);
        webContentsObserver.navigationEntryCommitted(details);

        // Scroll down to 10%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 10));
        gestureStateListener.onScrollEnded(10, SCROLL_EXTENT);

        // Verify % isn't reported.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());
    }

    @Test
    public void doesNotResetMaxOnNavigation_SubFrame_NewDocument() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        WebContentsObserver webContentsObserver = captureWebContentsObserver();

        // Scroll down to 90%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 90));
        gestureStateListener.onScrollEnded(90, SCROLL_EXTENT);

        // Verify 90% is reported.
        verify(env.connection).notifyGreatestScrollPercentageIncreased(eq(env.session), eq(90));
        clearInvocations(env.connection);

        LoadCommittedDetails details = new LoadCommittedDetails(0, JUnitTestGURLs.getGURL(URL_1),
                false, /*isSameDocument=*/false, /*isMainFrame=*/false, 200);
        webContentsObserver.navigationEntryCommitted(details);

        // Scroll down to 50%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 50));
        gestureStateListener.onScrollEnded(50, SCROLL_EXTENT);

        // Verify % isn't reported.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());
    }

    @Test
    public void returnsRetroactiveMaxScroll() {
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        Supplier<Integer> scrollPercentageSupplier = captureGreatestScrollPercentageSupplier();

        // Scroll down to 58%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 58));
        gestureStateListener.onScrollEnded(58, SCROLL_EXTENT);

        assertEquals(Integer.valueOf(55), scrollPercentageSupplier.get());
    }

    @Test
    public void returnsRetroactiveMaxScroll_zeroIfNotScrolled() {
        initializeTabForTest();
        Supplier<Integer> scrollPercentageSupplier = captureGreatestScrollPercentageSupplier();

        assertEquals(Integer.valueOf(0), scrollPercentageSupplier.get());
    }

    @Test
    public void returnsRetroactiveMaxScroll_nullIfNotReportingUsage() {
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        initializeTabForTest();

        Supplier<Integer> scrollPercentageSupplier = captureGreatestScrollPercentageSupplier();
        assertNull(scrollPercentageSupplier.get());
    }

    @Test
    public void returnsRetroactiveMaxScroll_zeroIfSendingFakeValues() {
        forceSendFakeValues();
        initializeTabForTest();
        GestureStateListener gestureStateListener = captureGestureStateListener();
        Supplier<Integer> scrollPercentageSupplier = captureGreatestScrollPercentageSupplier();

        // Scroll down to 46%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 46));
        gestureStateListener.onScrollEnded(46, SCROLL_EXTENT);

        assertEquals(Integer.valueOf(0), scrollPercentageSupplier.get());
    }

    @Test
    public void sendsFalseForScrollDirectionIfSendingFakeValues() {
        forceSendFakeValues();
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        verify(env.connection).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // Change direction to up at 10%.
        listener.onVerticalScrollDirectionChanged(true, .1f);
        verify(env.connection, times(2)).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // Change direction to down at 5%.
        listener.onVerticalScrollDirectionChanged(false, .05f);
        verify(env.connection, times(3)).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // End scrolling at 50%.
        listener.onScrollEnded(50, SCROLL_EXTENT);
        // We shouldn't make any more calls.
        verify(env.connection, times(3)).notifyVerticalScrollEvent(eq(env.session), anyBoolean());
    }

    @Test
    public void sendsZeroForMaxScrollSignalsIfSendingFakeValues() {
        forceSendFakeValues();
        initializeTabForTest();
        GestureStateListener listener = captureGestureStateListener();

        // Start by scrolling down.
        listener.onScrollStarted(0, SCROLL_EXTENT, false);
        // Scroll down to 3%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 3));
        // End scrolling.
        listener.onScrollEnded(3, SCROLL_EXTENT);
        // We shouldn't make any calls at this point.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());

        // Start scrolling down again.
        listener.onScrollStarted(3, SCROLL_EXTENT, false);
        // Scroll down to 8%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 8));
        // End scrolling.
        listener.onScrollEnded(8, SCROLL_EXTENT);
        // We should make a call, but it will be 0.
        verify(env.connection, times(1))
                .notifyGreatestScrollPercentageIncreased(eq(env.session), eq(0));

        // Start scrolling down again.
        listener.onScrollStarted(8, SCROLL_EXTENT, false);
        // Scroll down to 94%.
        listener.onScrollUpdateGestureConsumed(new Point(0, 94));
        // End scrolling.
        listener.onScrollEnded(94, SCROLL_EXTENT);
        // We should make a call, 0 again.
        verify(env.connection, times(2))
                .notifyGreatestScrollPercentageIncreased(eq(env.session), eq(0));
    }

    private void forceSendFakeValues() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS, REAL_VALUES, "false");
        FeatureList.setTestValues(testValues);
    }

    private void initializeTabForTest() {
        Tab initialTab = env.prepareTab();
        doAnswer(invocation -> {
            CustomTabTabObserver observer = invocation.getArgument(0);
            initialTab.addObserver(observer);
            observer.onAttachedToInitialTab(initialTab);
            return null;
        })
                .when(env.tabObserverRegistrar)
                .registerActivityTabObserver(any());

        mEngagementSignalObserver = new RealtimeEngagementSignalObserver(
                env.tabObserverRegistrar, env.connection, env.session);
        verify(env.tabObserverRegistrar).registerActivityTabObserver(mEngagementSignalObserver);

        env.tabProvider.setInitialTab(initialTab, TabCreationMode.DEFAULT);
    }

    private GestureStateListener captureGestureStateListener() {
        ArgumentCaptor<GestureStateListener> gestureStateListenerArgumentCaptor =
                ArgumentCaptor.forClass(GestureStateListener.class);
        verify(mGestureListenerManagerImpl, atLeastOnce())
                .addListener(gestureStateListenerArgumentCaptor.capture());
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

    private Supplier<Integer> captureGreatestScrollPercentageSupplier() {
        ArgumentCaptor<Supplier<Integer>> greatestScrollPercentageSupplierArgumentCaptor =
                ArgumentCaptor.forClass(Supplier.class);
        verify(env.connection)
                .setGreatestScrollPercentageSupplier(
                        greatestScrollPercentageSupplierArgumentCaptor.capture());
        return greatestScrollPercentageSupplierArgumentCaptor.getValue();
    }

    private void verifyNoMemoryLeakForGestureStateListener(GestureStateListener listener) {
        listener.onScrollStarted(0, 1, false);
        listener.onScrollUpdateGestureConsumed(new Point(1, 1));
        listener.onVerticalScrollDirectionChanged(false, 0.1f);
        listener.onScrollEnded(1, 0);
    }
}
