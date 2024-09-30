// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.net.Network;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.cookies.CookiesFetcherJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetId;

/** Tests for {@link CustomTabActivityTabController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class})
@Features.EnableFeatures(ChromeFeatureList.CCT_PREWARM_TAB)
public class CustomTabActivityTabControllerUnitTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Rule public final JniMocker jniMocker = new JniMocker();

    private CustomTabActivityTabController mTabController;

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    @Mock private Network mNetwork;

    @Mock private CookiesFetcher.Natives mCookiesFetcherJni;

    private static final long TEST_TARGET_NETWORK = 1000;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(env.profileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(env.profileProvider.getOffTheRecordProfile(eq(true))).thenReturn(mIncognitoProfile);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(env.intentDataProvider.getTargetNetwork()).thenReturn((long) NetId.INVALID);

        mTabController = env.createTabController();
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManager);

        jniMocker.mock(CookiesFetcherJni.TEST_HOOKS, mCookiesFetcherJni);
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
    public void usesRestoredTab_IfOffTheRecord_IfAvailable() {
        env.isOffTheRecord = true;
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        when(env.cipherFactory.restoreFromBundle(any())).thenReturn(true);
        env.reachNativeInit(mTabController);
        assertEquals(savedTab, env.tabProvider.getTab());
        assertEquals(TabCreationMode.RESTORED, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void doesntUseRestoredTab_IfOffTheRecord_NoCipherKey() {
        env.isOffTheRecord = true;
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        when(env.cipherFactory.restoreFromBundle(any())).thenReturn(false);
        env.reachNativeInit(mTabController);
        assertEquals(env.tabFromFactory, env.tabProvider.getTab());
        assertEquals(TabCreationMode.DEFAULT, env.tabProvider.getInitialTabCreationMode());
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
        when(env.webContentsFactory.createWebContentsWithWarmRenderer(
                        any(), anyBoolean(), anyLong()))
                .thenReturn(webContents);
        env.reachNativeInit(mTabController);
        assertEquals(webContents, env.webContentsCaptor.getValue());
    }

    @Test
    public void propagatesTargetNetworkCorrectly_whenIntentDataProviderTargetsNetwork() {
        when(env.intentDataProvider.getTargetNetwork()).thenReturn(TEST_TARGET_NETWORK);
        env.reachNativeInit(mTabController);
        verify(env.webContentsFactory, never())
                .createWebContentsWithWarmRenderer(
                        any(), anyBoolean(), not(eq(TEST_TARGET_NETWORK)));
        verify(env.webContentsFactory)
                .createWebContentsWithWarmRenderer(any(), anyBoolean(), eq(TEST_TARGET_NETWORK));
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
        assertNotEquals(spareWebcontents, env.webContentsCaptor.getValue());
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
        doAnswer(
                        (mock) -> {
                            fail(
                                    "setClientDataHeaderForNewTab() should not be called for"
                                            + " incognito tabs");
                            return null;
                        })
                .when(env.connection)
                .setClientDataHeaderForNewTab(any(), any());
        env.isOffTheRecord = true;
        mTabController.onPreInflationStartup();
        mTabController.finishNativeInitialization();
        Tab tab = env.prepareTab();
        assertTrue(tab.isOffTheRecord());
    }

    @Test
    public void setsTabObserverRegistrarOnEngagementSignalsHandler() {
        var handler = mock(EngagementSignalsHandler.class);
        when(env.connection.getEngagementSignalsHandler(eq(env.session))).thenReturn(handler);
        when(env.connection.isDynamicFeatureEnabled(anyString())).thenReturn(true);
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        env.reachNativeInit(mTabController);
        verify(handler).setTabObserverRegistrar(env.tabObserverRegistrar);
    }
}
