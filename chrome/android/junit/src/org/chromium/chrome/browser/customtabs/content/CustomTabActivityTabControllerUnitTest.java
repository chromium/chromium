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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.cookies.CookiesFetcherJni;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetId;

/** Tests for {@link CustomTabActivityTabController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class})
public class CustomTabActivityTabControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    private CustomTabActivityTabController mTabController;

    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    @Mock private UserPrefsJni mMockUserPrefsJni;

    @Mock private CookiesFetcher.Natives mCookiesFetcherJni;

    private static final long TEST_TARGET_NETWORK = 1000;

    @Before
    public void setUp() {
        when(env.intentDataProvider.getTargetNetwork()).thenReturn((long) NetId.INVALID);

        // Ensure the test can read the Autofill pref. Assume it's turned off by default.
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
        UserPrefsJni.setInstanceForTesting(mMockUserPrefsJni);
        doReturn(mock(PrefService.class)).when(mMockUserPrefsJni).get(any());

        mTabController = spy(env.createTabController());
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManager);

        CookiesFetcherJni.setInstanceForTesting(mCookiesFetcherJni);
    }

    @Test
    public void createsTabEarly_IfWarmUpIsFinished() {
        env.warmUp();
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertNotNull(env.tabProvider.getTab());
        assertEquals(TabCreationMode.EARLY, env.tabProvider.getInitialTabCreationMode());
    }

    // Some websites replace the tab with a new one.
    @Test
    public void returnsNewTab_IfTabChanges() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        mTabController.finishNativeInitialization();
        Tab newTab = env.prepareTab();
        env.changeTab(newTab);
        assertEquals(newTab, env.tabProvider.getTab());
    }

    @Test
    public void usesRestoredTab_IfAvailable() {
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(savedTab, env.tabProvider.getTab());
        assertEquals(TabCreationMode.RESTORED, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void usesRestoredTab_IfOffTheRecord_IfAvailable() {
        env.isOffTheRecord = true;
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        when(env.cipherFactory.restoreFromBundle(any())).thenReturn(true);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(savedTab, env.tabProvider.getTab());
        assertEquals(TabCreationMode.RESTORED, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void doesntUseRestoredTab_IfOffTheRecord_NoCipherKey() {
        env.isOffTheRecord = true;
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        when(env.cipherFactory.restoreFromBundle(any())).thenReturn(false);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(env.tabFromFactory, env.tabProvider.getTab());
        assertEquals(TabCreationMode.DEFAULT, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void doesntCreateNewTab_IfRestored() {
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        verify(env.tabFactory, never()).createTab(any(), any(), any());
    }

    @Test
    public void createsANewTabOnNativeInit_IfNoTabExists() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(env.tabFromFactory, env.tabProvider.getTab());
        assertEquals(TabCreationMode.DEFAULT, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void doesntCreateNewTabOnNativeInit_IfCreatedTabEarly() {
        env.warmUp();
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();

        clearInvocations(env.tabFactory);
        mTabController.finishNativeInitialization();
        verify(env.tabFactory, never()).createTab(any(), any(), any());
    }

    @Test
    public void addsEarlyCreatedTab_ToTabModel() {
        env.warmUp();
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        verify(env.tabModel).addTab(eq(env.tabFromFactory), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void addsTabCreatedOnNativeInit_ToTabModel() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        verify(env.tabModel).addTab(eq(env.tabFromFactory), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void usesHiddenTab_IfAvailable() {
        Tab hiddenTab = env.prepareHiddenTab();
        mTabController.setUpInitialTab(hiddenTab);
        mTabController.finishNativeInitialization();
        assertEquals(hiddenTab, env.tabProvider.getTab());
        assertEquals(TabCreationMode.HIDDEN, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void finishesReparentingHiddenTab() {
        Tab hiddenTab = env.prepareHiddenTab();
        mTabController.setUpInitialTab(hiddenTab);
        mTabController.finishNativeInitialization();
        verify(env.reparentingTask).finish(any(), any());
    }

    @Test
    public void usesWebContentsCreatedWithWarmRenderer_ByDefault() {
        WebContents webContents = mock(WebContents.class);
        when(env.mWebContentsFactoryJni.createWebContents(
                        /* profile= */ any(),
                        /* initiallyHidden= */ anyBoolean(),
                        /* initializeRenderer= */ eq(true),
                        /* usesPlatformAutofill= */ eq(false),
                        /* targetNetwork= */ anyLong(),
                        any()))
                .thenReturn(webContents);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(webContents, env.webContentsCaptor.getValue());
    }

    @Test
    public void usesWebContentsCreatedWithWarmRenderer_whenUsersOptInto3pAutofill() {
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                AndroidAutofillAvailabilityStatus.AVAILABLE);
        WebContents webContents = mock(WebContents.class);
        when(env.mWebContentsFactoryJni.createWebContents(
                        /* profile= */ any(),
                        /* initiallyHidden= */ anyBoolean(),
                        /* initializeRenderer= */ eq(true),
                        /* usesPlatformAutofill= */ eq(true),
                        /* targetNetwork= */ anyLong(),
                        any()))
                .thenReturn(webContents);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(webContents, env.webContentsCaptor.getValue());
    }

    @Test
    public void propagatesTargetNetworkCorrectly_whenIntentDataProviderTargetsNetwork() {
        when(env.intentDataProvider.getTargetNetwork()).thenReturn(TEST_TARGET_NETWORK);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        verify(env.mWebContentsFactoryJni, never())
                .createWebContents(
                        /* profile= */ any(),
                        /* initiallyHidden= */ anyBoolean(),
                        /* initializeRenderer= */ eq(true),
                        /* usesPlatformAutofill= */ eq(false),
                        /* targetNetwork= */ not(eq(TEST_TARGET_NETWORK)),
                        any());
        verify(env.mWebContentsFactoryJni)
                .createWebContents(
                        /* profile= */ any(),
                        /* initiallyHidden= */ anyBoolean(),
                        /* initializeRenderer= */ eq(true),
                        /* usesPlatformAutofill= */ eq(false),
                        /* targetNetwork= */ eq(TEST_TARGET_NETWORK),
                        any());
    }

    @Test
    public void createsWebContentsFromScratch_whenIntentDataProviderTargetsNetwork() {
        WebContents webContents = mock(WebContents.class);
        when(env.intentDataProvider.getTargetNetwork()).thenReturn(TEST_TARGET_NETWORK);
        when(env.mWebContentsFactoryJni.createWebContents(
                        /* profile= */ any(),
                        /* initiallyHidden= */ anyBoolean(),
                        /* initializeRenderer= */ eq(true),
                        /* usesPlatformAutofill= */ eq(false),
                        /* targetNetwork= */ eq(TEST_TARGET_NETWORK),
                        any()))
                .thenReturn(webContents);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        // CustomTabActivityTabController#takeWebContents is the only entrypoint that can correctly
        // handle IntentDataProvider#targetNetwork. As such, we expect it to always create a
        // WebContents, targeting that network, when a network is specified in the intent.
        // What does this mean? In practice, with the current code layout, we expect
        // CustomTabActivityTabFactory#createTab to
        // be called with a pre-existing WebContents. More specifically, we expect
        // WebContentsFactory#createWebContentsWithWarmRenderer, within
        // CustomTabActivityTabController#takeWebContents to have handled that (for the reasons
        // stated above).
        // Note: This is a lot of ifs and poking into internal implementation details, which is
        // definitely not ideal. Unfortunately, this is the best we could come up to confirm that
        // IntentDataProvider#targetNetwork is being correctly handled.
        verify(env.tabFactory, never())
                .createTab(
                        /* webContents= */ eq(null),
                        /* delegateFactory= */ any(),
                        /* action= */ any());
        verify(env.tabFactory)
                .createTab(
                        /* webContents= */ eq(webContents),
                        /* delegateFactory= */ any(),
                        /* action= */ any());
        assertEquals(webContents, env.webContentsCaptor.getValue());
    }

    @Test
    public void usesTransferredWebContents_IfAvailable() {
        WebContents transferredWebcontents = env.prepareTransferredWebcontents();
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(transferredWebcontents, env.webContentsCaptor.getValue());
    }

    // This is important so that the tab doesn't get hidden, see ChromeActivity#onStopWithNative
    @Test
    public void clearsActiveTab_WhenStartsReparenting() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
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
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        mTabController.finishNativeInitialization();
        Tab tab = env.prepareTab();
        assertTrue(tab.isOffTheRecord());
    }

    @Test
    public void setsTabObserverRegistrarOnEngagementSignalsHandler() {
        var handler = mock(EngagementSignalsHandler.class);
        when(env.connection.getEngagementSignalsHandler(eq(env.session))).thenReturn(handler);
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        verify(handler).setTabObserverRegistrar(env.tabObserverRegistrar);
    }

    @Test
    public void usesTabFromIntent_IfAvailable() {
        Tab tab = env.prepareTransferredTab();
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(tab, env.tabProvider.getTab());
    }

    @Test
    public void doesNotUseTabFromIntent_IfNotInAsyncParamsManager() {
        Tab tab = env.prepareTransferredTab();
        AsyncTabParamsManagerSingleton.getInstance().remove(tab.getId());
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertNotEquals(tab, env.tabProvider.getTab());
    }

    // If the Activity has been recreated, ignore the Tab ID provided in the Intent -- the Tab will
    // be restored using a different mechanism. See crbug.com/448865648.
    @Test
    public void doesNotUseTabFromIntent_IfActivityRecreated() {
        Tab popupTab = env.prepareTransferredTab();
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertEquals(savedTab, env.tabProvider.getTab());
        assertEquals(TabCreationMode.RESTORED, env.tabProvider.getInitialTabCreationMode());
    }

    @Test
    public void getTabCount_noTabs() {
        when(env.tabModel.getCount()).thenReturn(0);
        assertEquals(0, mTabController.getTabCount());
    }

    @Test
    public void getTabCount_oneTab() {
        when(env.tabModel.getCount()).thenReturn(1);
        assertEquals(1, mTabController.getTabCount());
    }

    @Test
    public void getTabCount_multipleTabs() {
        when(env.tabModel.getCount()).thenReturn(5);
        assertEquals(5, mTabController.getTabCount());
    }

    @Test
    public void updatesIntentInTab_WhenNotWebapp() {
        env.warmUp();
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertNotNull(env.tabProvider.getTab());

        verify(mTabController).updateIntentInTab(eq(env.tabProvider.getTab()), eq(true));
        // Verify that RedirectHandlerTabHelper did not need to ask the tab if it was a custom tab.
        verify(env.tabFromFactory, never()).isCustomTab();
    }

    @Test
    public void doesNotUpdateIntentInTab_WhenIsWebapp() {
        env.warmUp();
        when(env.intentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        assertNotNull(env.tabProvider.getTab());

        verify(mTabController, never()).updateIntentInTab(any(), anyBoolean());
    }
}
