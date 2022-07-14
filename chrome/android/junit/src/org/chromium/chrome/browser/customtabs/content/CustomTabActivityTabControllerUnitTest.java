// Copyright 2018 The Chromium Authors. All rights reserved.
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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;

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

    private CustomTabActivityTabController mTabController;

    @Mock
    private Profile mProfile;
    @Mock
    private GestureListenerManagerImpl mGestureListenerManagerImpl;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        mTabController = env.createTabController();
        GestureListenerManagerImpl.setInstanceForTesting(mGestureListenerManagerImpl);
    }

    @After
    public void tearDown() {
        GestureListenerManagerImpl.setInstanceForTesting(null);
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
    public void addsListenersForSignalsIfFeatureIsEnabled() {
        env.reachNativeInit(mTabController);

        verify(mGestureListenerManagerImpl).addListener(any(GestureStateListener.class));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void removesGestureStateListenerWhenWebContentsWillSwap() {
        env.reachNativeInit(mTabController);

        ArgumentCaptor<GestureStateListener> gestureStateListenerArgumentCaptor =
                ArgumentCaptor.forClass(GestureStateListener.class);
        verify(mGestureListenerManagerImpl)
                .addListener(gestureStateListenerArgumentCaptor.capture());

        ArgumentCaptor<TabObserver> tabObserverArgumentCaptor =
                ArgumentCaptor.forClass(TabObserver.class);
        verify(env.tabProvider.getTab(), atLeastOnce())
                .addObserver(tabObserverArgumentCaptor.capture());

        for (TabObserver observer : tabObserverArgumentCaptor.getAllValues()) {
            observer.webContentsWillSwap(env.tabProvider.getTab());
        }
        verify(mGestureListenerManagerImpl)
                .removeListener(gestureStateListenerArgumentCaptor.getValue());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendsSignalsForScrollStartThenEnd() {
        env.reachNativeInit(mTabController);

        ArgumentCaptor<GestureStateListener> gestureStateListenerArgumentCaptor =
                ArgumentCaptor.forClass(GestureStateListener.class);
        verify(mGestureListenerManagerImpl)
                .addListener(gestureStateListenerArgumentCaptor.capture());

        // Start scrolling down.
        gestureStateListenerArgumentCaptor.getValue().onScrollStarted(0, 100, false);
        verify(env.connection).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // End scrolling at 50%.
        gestureStateListenerArgumentCaptor.getValue().onScrollEnded(50, 100);
        // We shouldn't make any more calls.
        verify(env.connection, times(1)).notifyVerticalScrollEvent(eq(env.session), anyBoolean());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void sendsSignalsForScrollStartDirectionChangeThenEnd() {
        env.reachNativeInit(mTabController);

        ArgumentCaptor<GestureStateListener> gestureStateListenerArgumentCaptor =
                ArgumentCaptor.forClass(GestureStateListener.class);
        verify(mGestureListenerManagerImpl)
                .addListener(gestureStateListenerArgumentCaptor.capture());

        // Start by scrolling down.
        gestureStateListenerArgumentCaptor.getValue().onScrollStarted(0, 100, false);
        verify(env.connection).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // Change direction to up at 10%.
        gestureStateListenerArgumentCaptor.getValue().onVerticalScrollDirectionChanged(true, .1f);
        verify(env.connection).notifyVerticalScrollEvent(eq(env.session), eq(true));
        // Change direction to down at 5%.
        gestureStateListenerArgumentCaptor.getValue().onVerticalScrollDirectionChanged(false, .05f);
        verify(env.connection, times(2)).notifyVerticalScrollEvent(eq(env.session), eq(false));
        // End scrolling at 50%.
        gestureStateListenerArgumentCaptor.getValue().onScrollEnded(50, 100);
        // We shouldn't make any more calls.
        verify(env.connection, times(3)).notifyVerticalScrollEvent(eq(env.session), anyBoolean());
    }
}
