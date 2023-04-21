// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
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
    private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        mTabController = env.createTabController();
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManager);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
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
    @Features.DisableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void doesNotSetGreatestScrollPercentageSupplierIfFeatureIsDisabled() {
        env.reachNativeInit(mTabController);

        ArgumentCaptor<TabObserver> tabObservers = ArgumentCaptor.forClass(TabObserver.class);
        verify(env.tabObserverRegistrar, atLeastOnce()).registerTabObserver(tabObservers.capture());
        for (TabObserver observer : tabObservers.getAllValues()) {
            assertFalse("RealtimeEngagementSignalObserver is not attached.",
                    observer instanceof RealtimeEngagementSignalObserver);
        }
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    @Features.DisableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS_ALTERNATIVE_IMPL})
    public void attachEngagementSignalObserver() {
        when(env.connection.isDynamicFeatureEnabled(anyString())).thenReturn(true);
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        env.reachNativeInit(mTabController);

        ArgumentCaptor<CustomTabTabObserver> tabObservers =
                ArgumentCaptor.forClass(CustomTabTabObserver.class);
        verify(env.tabObserverRegistrar, atLeastOnce())
                .registerActivityTabObserver(tabObservers.capture());
        for (TabObserver observer : tabObservers.getAllValues()) {
            if (observer instanceof RealtimeEngagementSignalObserver) {
                return;
            }
        }
        throw new AssertionError("RealtimeEngagementSignalObserver is not attached.");
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS_ALTERNATIVE_IMPL})
    public void disableUsageAndCrashReporting_BeforeInit() {
        when(env.connection.isDynamicFeatureEnabled(anyString())).thenReturn(true);
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(false);
        env.reachNativeInit(mTabController);

        assertNull(mTabController.getRealtimeEngagementSignalObserverForTesting());
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS_ALTERNATIVE_IMPL})
    public void disableUsageAndCrashReporting_AfterInit() {
        when(env.connection.isDynamicFeatureEnabled(anyString())).thenReturn(true);
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        env.reachNativeInit(mTabController);

        assertNotNull(mTabController.getRealtimeEngagementSignalObserverForTesting());

        ArgumentCaptor<PrivacyPreferencesManager.Observer> observer =
                ArgumentCaptor.forClass(PrivacyPreferencesManager.Observer.class);
        verify(mPrivacyPreferencesManager).addObserver(observer.capture());

        observer.getValue().onIsUsageAndCrashReportingPermittedChanged(false);
        verify(env.connection).notifyDidGetUserInteraction(eq(env.session), eq(false));
        assertNull(mTabController.getRealtimeEngagementSignalObserverForTesting());
    }
}
