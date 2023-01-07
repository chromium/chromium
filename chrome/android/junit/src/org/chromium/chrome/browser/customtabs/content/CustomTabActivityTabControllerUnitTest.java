// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
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

import static org.chromium.url.JUnitTestGURLs.URL_1;

import android.content.Intent;
import android.graphics.Point;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/**
 * Tests for {@link CustomTabActivityTabController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class})
@DisableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
public class CustomTabActivityTabControllerUnitTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Rule
    public Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private static final int SCROLL_EXTENT = 100;

    private CustomTabActivityTabController mTabController;

    @Mock
    private Profile mProfile;
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
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        mTabController = env.createTabController();
        when(mRenderCoordinatesImpl.getMaxVerticalScrollPixInt()).thenReturn(100);
        GestureListenerManagerImpl.setInstanceForTesting(mGestureListenerManagerImpl);
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinatesImpl);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerImpl);
        TabInteractionRecorder.setInstanceForTesting(mTabInteractionRecorder);
        doReturn(true).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
    }

    @After
    public void tearDown() {
        RenderCoordinatesImpl.setInstanceForTesting(null);
        GestureListenerManagerImpl.setInstanceForTesting(null);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(null);
        TabInteractionRecorder.setInstanceForTesting(null);
    }

    @Test
    public void createsTabEarly_IfWarmUpIsFinished() {
        env.warmUp();
        mTabController.onPreInflationStartup();
        assertNotNull(env.tabProvider.getTab());
        assertEquals(TabCreationMode.EARLY, env.tabProvider.getInitialTabCreationMode());
    }

    // Some websites replace the tab with a new one.
    @Test
    public void returnsNewTab_IfTabChanges() {
        mTabController.onPreInflationStartup();
        mTabController.finishNativeInitialization();
        Tab newTab = env.prepareTab();
        env.changeTab(newTab);
        assertEquals(newTab, env.tabProvider.getTab());
    }

    @Test
    public void usesRestoredTab_IfAvailable() {
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        env.reachNativeInit(mTabController);
        assertEquals(savedTab, env.tabProvider.getTab());
        assertEquals(TabCreationMode.RESTORED, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void doesntCreateNewTab_IfRestored() {
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        env.reachNativeInit(mTabController);
        verify(env.tabFactory, never()).createTab(any(), any(), any());
    }

    @Test
    public void createsANewTabOnNativeInit_IfNoTabExists() {
        env.reachNativeInit(mTabController);
        assertEquals(env.tabFromFactory, env.tabProvider.getTab());
        assertEquals(TabCreationMode.DEFAULT, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void doesntCreateNewTabOnNativeInit_IfCreatedTabEarly() {
        env.warmUp();
        mTabController.onPreInflationStartup();

        clearInvocations(env.tabFactory);
        mTabController.finishNativeInitialization();
        verify(env.tabFactory, never()).createTab(any(), any(), any());
    }

    @Test
    public void addsEarlyCreatedTab_ToTabModel() {
        env.warmUp();
        env.reachNativeInit(mTabController);
        verify(env.tabModel).addTab(eq(env.tabFromFactory), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void addsTabCreatedOnNativeInit_ToTabModel() {
        env.reachNativeInit(mTabController);
        verify(env.tabModel).addTab(eq(env.tabFromFactory), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void usesHiddenTab_IfAvailable() {
        Tab hiddenTab = env.prepareHiddenTab();
        env.reachNativeInit(mTabController);
        assertEquals(hiddenTab, env.tabProvider.getTab());
        assertEquals(TabCreationMode.HIDDEN, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void finishesReparentingHiddenTab() {
        Tab hiddenTab = env.prepareHiddenTab();
        env.reachNativeInit(mTabController);
        verify(env.reparentingTaskProvider.get(hiddenTab)).finish(any(), any());
    }

    @Test
    public void usesWebContentsCreatedWithWarmRenderer_ByDefault() {
        WebContents webContents = mock(WebContents.class);
        when(env.webContentsFactory.createWebContentsWithWarmRenderer(any(), anyBoolean()))
                .thenReturn(webContents);
        env.reachNativeInit(mTabController);
        assertEquals(webContents, env.webContentsCaptor.getValue());
    }

    @Test
    public void usesTransferredWebContents_IfAvailable() {
        WebContents transferredWebcontents = env.prepareTransferredWebcontents();
        env.reachNativeInit(mTabController);
        assertEquals(transferredWebcontents, env.webContentsCaptor.getValue());
    }

    @Test
    public void usesSpareWebContents_IfAvailable() {
        WebContents spareWebcontents = env.prepareSpareWebcontents();
        env.reachNativeInit(mTabController);
        assertEquals(spareWebcontents, env.webContentsCaptor.getValue());
    }

    @Test
    public void prefersTransferredWebContents_ToSpareWebContents() {
        WebContents transferredWebcontents = env.prepareTransferredWebcontents();
        WebContents spareWebcontents = env.prepareSpareWebcontents();
        env.reachNativeInit(mTabController);
        assertEquals(transferredWebcontents, env.webContentsCaptor.getValue());
    }

    // This is important so that the tab doesn't get hidden, see ChromeActivity#onStopWithNative
    @Test
    public void clearsActiveTab_WhenStartsReparenting() {
        env.reachNativeInit(mTabController);
        mTabController.detachAndStartReparenting(new Intent(), new Bundle(), mock(Runnable.class));
        assertNull(env.tabProvider.getTab());
    }

    // Some websites replace the tab with a new one.
    @Test
    public void doesNotSetHeaderWhenIncognito() {
        doAnswer((mock) -> {
            fail("setClientDataHeaderForNewTab() should not be called for incognito tabs");
            return null;
        })
                .when(env.connection)
                .setClientDataHeaderForNewTab(any(), any());
        env.isIncognito = true;
        mTabController.onPreInflationStartup();
        mTabController.finishNativeInitialization();
        Tab tab = env.prepareTab();
        assertTrue(tab.isIncognito());
    }

    @Test
    public void doesNotAddListenersForSignalsIfFeatureIsDisabled() {
        env.reachNativeInit(mTabController);

        verify(mGestureListenerManagerImpl, never()).addListener(any(GestureStateListener.class));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotAddListenersForSignalsIfUmaUploadIsDisabled() {
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        env.reachNativeInit(mTabController);

        verify(mGestureListenerManagerImpl, never()).addListener(any(GestureStateListener.class));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void addsListenersForSignalsIfFeatureIsEnabled() {
        env.reachNativeInit(mTabController);

        verify(mGestureListenerManagerImpl).addListener(any(GestureStateListener.class));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void removesGestureStateListenerWhenWebContentsWillSwap() {
        env.reachNativeInit(mTabController);
        GestureStateListener listener = captureGestureStateListener();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.webContentsWillSwap(env.tabProvider.getTab());
        }
        verify(mGestureListenerManagerImpl).removeListener(listener);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendUserInteractionOnTabDestroyed_NoUserInteraction() {
        env.reachNativeInit(mTabController);
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection).notifyDidGetUserInteraction(eq(env.session), eq(false));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendUserInteractionOnTabDestroyed_DidGetUserInteraction() {
        env.reachNativeInit(mTabController);
        doReturn(true).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection).notifyDidGetUserInteraction(eq(env.session), eq(true));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendUserInteractionOnTabHidden() {
        env.reachNativeInit(mTabController);
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onHidden(env.tabProvider.getTab(), TabHidingType.ACTIVITY_HIDDEN);
        }
        verify(env.connection).notifyDidGetUserInteraction(eq(env.session), eq(false));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendUserInteractionOnTabHidden_OtherReason() {
        env.reachNativeInit(mTabController);
        doReturn(false).when(mTabInteractionRecorder).didGetUserInteraction();
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onHidden(env.tabProvider.getTab(), TabHidingType.CHANGED_TABS);
            observer.onHidden(env.tabProvider.getTab(), TabHidingType.REPARENTED);
        }
        verify(env.connection, never()).notifyDidGetUserInteraction(eq(env.session), eq(false));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotSendUserInteractionWhenIncognito() {
        env.isIncognito = true;
        env.reachNativeInit(mTabController);
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection, never()).notifyDidGetUserInteraction(eq(env.session), anyBoolean());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotSendUserInteractionWhenUmaUploadDisabled() {
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        env.reachNativeInit(mTabController);
        List<TabObserver> tabObservers = captureTabObservers();
        for (TabObserver observer : tabObservers) {
            observer.onDestroyed(env.tabProvider.getTab());
        }
        verify(env.connection, never()).notifyDidGetUserInteraction(eq(env.session), anyBoolean());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendsSignalsForScrollStartThenEnd() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendsSignalsForScrollStartDirectionChangeThenEnd() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotSendMaxScrollSignalForZeroPercent() {
        env.reachNativeInit(mTabController);

        // We shouldn't make any calls.
        verify(env.connection, never())
                .notifyGreatestScrollPercentageIncreased(eq(env.session), anyInt());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void onlySendsMaxScrollSignalAfterScrollEnd() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void onlySendsMaxScrollSignalForFivesMultiples() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotSendSignalForLowerPercentage() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotSendSignalEqualToPreviousMax() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void resetsMaxOnNavigation_MainFrame_NewDocument() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotResetMaxOnNavigation_MainFrame_SameDocument() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotResetMaxOnNavigation_SubFrame_NewDocument() {
        env.reachNativeInit(mTabController);
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void returnsRetroactiveMaxScroll() {
        env.reachNativeInit(mTabController);
        GestureStateListener gestureStateListener = captureGestureStateListener();
        Supplier<Integer> scrollPercentageSupplier = captureGreatestScrollPercentageSupplier();

        // Scroll down to 58%.
        gestureStateListener.onScrollStarted(0, SCROLL_EXTENT, false);
        gestureStateListener.onScrollUpdateGestureConsumed(new Point(0, 58));
        gestureStateListener.onScrollEnded(58, SCROLL_EXTENT);

        assertEquals(Integer.valueOf(55), scrollPercentageSupplier.get());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void returnsRetroactiveMaxScroll_zeroIfNotScrolled() {
        env.reachNativeInit(mTabController);
        Supplier<Integer> scrollPercentageSupplier = captureGreatestScrollPercentageSupplier();

        assertEquals(Integer.valueOf(0), scrollPercentageSupplier.get());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void returnsRetroactiveMaxScroll_nullIfNotReportingUsage() {
        doReturn(false).when(mPrivacyPreferencesManagerImpl).isUsageAndCrashReportingPermitted();
        env.reachNativeInit(mTabController);
        Supplier<Integer> scrollPercentageSupplier = captureGreatestScrollPercentageSupplier();
        assertNull(scrollPercentageSupplier.get());
    }

    @Test
    public void doesNotSetGreatestScrollPercentageSupplierIfFeatureIsDisabled() {
        env.reachNativeInit(mTabController);

        verify(env.connection, never()).setGreatestScrollPercentageSupplier(any());
    }

    private GestureStateListener captureGestureStateListener() {
        ArgumentCaptor<GestureStateListener> gestureStateListenerArgumentCaptor =
                ArgumentCaptor.forClass(GestureStateListener.class);
        verify(mGestureListenerManagerImpl)
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
}
