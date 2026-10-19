// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.Application;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;

import androidx.browser.trusted.sharing.ShareData;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.components.webapps.WebappsUtilsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

/** JUnit test for WebappLauncherActivity. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class WebappLauncherActivityTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";
    private static final String SCOPE = "https://www.google.com/scope/";

    private TabWindowManager mMockTabWindowManager;
    private TabModel mMockTabModel;
    private TabCreator mMockTabCreator;
    private WebappsUtils.Natives mWebappsUtilsJni;

    /** {@link WebappLauncherActivity} that fails to start any activity. */
    private static class FailingStartWebappLauncherActivity extends WebappLauncherActivity {
        @Override
        public void startActivity(Intent intent, Bundle options) {
            throw new ActivityNotFoundException("Simulated startActivity failure");
        }
    }

    @Before
    public void setUp() {
        WebApkValidator.setDisableValidationForTesting(true);
        mWebappsUtilsJni = mock(WebappsUtils.Natives.class);
        WebappsUtilsJni.setInstanceForTesting(mWebappsUtilsJni);
        mMockTabWindowManager = mock(TabWindowManager.class);
        mMockTabModel = mock(TabModel.class);
        mMockTabCreator = mock(TabCreator.class);
        when(mMockTabModel.getTabCreator()).thenReturn(mMockTabCreator);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mMockTabWindowManager);
    }

    @After
    public void tearDown() {
        WebApkReparentingHandler.getInstance().clear();
    }

    private Tab createReparentableMockTab(int tabId) {
        Tab mockTab = mock(Tab.class);
        when(mockTab.getId()).thenReturn(tabId);
        when(mockTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(mockTab.isTabInBrowser()).thenReturn(true);
        when(mockTab.getWebContents()).thenReturn(mock(WebContents.class));
        when(mockTab.getUrl()).thenReturn(new GURL(START_URL));
        return mockTab;
    }

    /**
     * Test that WebappLauncherActivity modifies the passed-in intent so that
     * WebApkIntentDataProviderFactory#create() returns null if the intent does not refer to a valid
     * WebAPK.
     */
    @Test
    public void testTryCreateWebappInfoAltersIntentIfNotValidWebApk() {
        WebApkValidator.setDisableValidationForTesting(false);

        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        assertNotNull(WebApkIntentDataProviderFactory.create(intent));
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        assertNull(WebApkIntentDataProviderFactory.create(intent));
    }

    /** Test the launch intent created by {@link WebappLauncherActivity} for old-style WebAPKs. */
    @Test
    public void testOldStyleLaunchIntent() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertEquals(WebappActivity.class.getName(), launchIntent.getComponent().getClassName());
        // WebAPK package name should be part of the intent URI to enable launching multiple
        // WebAPKs.
        assertEquals("webapp://webapk-" + WEBAPK_PACKAGE_NAME, launchIntent.getDataString());
        assertTrue((launchIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
        assertFalse(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, IntentHandler.EXTRA_SKIP_LOAD_ON_REPARENTING, false));
        assertNotNull(WebApkIntentDataProviderFactory.create(launchIntent));
    }

    /** Test the launch intent created by {@link WebappLauncherActivity} for new-style WebAPKs. */
    @Test
    public void testNewStyleLaunchIntent() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertEquals(
                SameTaskWebApkActivity.class.getName(), launchIntent.getComponent().getClassName());
        assertEquals(0, launchIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        assertNotNull(WebApkIntentDataProviderFactory.create(launchIntent));
    }

    /**
     * Test that WebappLauncherActivity unpacks the reparenting token into a trusted tab ID, sets
     * EXTRA_SKIP_LOAD_ON_REPARENTING, and opens a replacement NTP before detaching when the tab is
     * the last remaining tab in its TabModel.
     */
    @Test
    public void testReparentingTokenUnpackedToTabId_OpensNtpWhenLastTabInModel() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        final int tabId = 10;
        Tab mockTab = createReparentableMockTab(tabId);
        when(mMockTabWindowManager.getTabModelForTab(mockTab)).thenReturn(mMockTabModel);
        when(mMockTabModel.getCount()).thenReturn(1);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        intent, mockTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        Intent launchIntent = getNextStartedActivity();
        assertEquals(tabId, IntentHandler.getTabId(launchIntent));
        assertTrue(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, IntentHandler.EXTRA_SKIP_LOAD_ON_REPARENTING, false));

        // The replacement NTP must be opened before detaching the tab so the TabModel never
        // transiently drops to zero tabs.
        InOrder inOrder = inOrder(mMockTabCreator, mockTab);
        inOrder.verify(mMockTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
        inOrder.verify(mockTab).updateAttachment(null, null);
        AsyncTabParamsManagerSingleton.getInstance().remove(tabId);
    }

    /**
     * Test that when other tabs remain in the originating TabModel, reparenting detaches the tab
     * without opening a replacement NTP.
     */
    @Test
    public void testReparentingDoesNotOpenNtpWhenOtherTabsRemainInModel() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        final int tabId = 11;
        Tab mockTab = createReparentableMockTab(tabId);
        when(mMockTabWindowManager.getTabModelForTab(mockTab)).thenReturn(mMockTabModel);
        when(mMockTabModel.getCount()).thenReturn(2);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        intent, mockTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        Intent launchIntent = getNextStartedActivity();
        assertEquals(tabId, IntentHandler.getTabId(launchIntent));
        verify(mockTab).updateAttachment(null, null);
        verify(mMockTabCreator, never()).launchNtp(anyInt());
        AsyncTabParamsManagerSingleton.getInstance().remove(tabId);
    }

    /**
     * Test that the reparenting token is one-shot: once a matching token has been received, state
     * is cleared even if reparenting does not proceed, and replaying the intent is a no-op.
     */
    @Test
    public void testReparentingTokenIsOneShot() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        final int tabId = 12;
        Tab mockTab = createReparentableMockTab(tabId);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        intent, mockTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);

        // First attempt with a mismatched URL consumes the matching token and clears state.
        Intent mismatchedUrlIntent = new Intent(intent);
        mismatchedUrlIntent.putExtra(
                WebappConstants.EXTRA_URL, "https://www.google.com/scope/other");
        assertEquals(
                Tab.INVALID_TAB_ID,
                WebApkReparentingHandler.getInstance().getTabIdForReparenting(mismatchedUrlIntent));

        // Second attempt with the original valid intent must now fail because the token was used.
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        Intent launchIntent = getNextStartedActivity();
        assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(launchIntent));
        assertFalse(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, IntentHandler.EXTRA_SKIP_LOAD_ON_REPARENTING, false));
        verify(mockTab, never()).updateAttachment(any(), any());
        verify(mMockTabCreator, never()).launchNtp(anyInt());
    }

    /**
     * Test that a bounce with a missing or mismatched token doesn't consume the pending state, so
     * the real bounce still reparents the tab.
     */
    @Test
    public void testReparentingStateKeptForMismatchedToken() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        final int tabId = 17;
        Tab mockTab = createReparentableMockTab(tabId);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        intent, mockTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);

        Intent noTokenIntent = new Intent(intent);
        noTokenIntent.removeExtra(WebApkReparentingHandler.EXTRA_WEBAPK_REPARENT_TOKEN);
        assertEquals(
                Tab.INVALID_TAB_ID,
                WebApkReparentingHandler.getInstance().getTabIdForReparenting(noTokenIntent));

        byte[] wrongToken =
                intent.getByteArrayExtra(WebApkReparentingHandler.EXTRA_WEBAPK_REPARENT_TOKEN)
                        .clone();
        wrongToken[0] ^= 1;
        Intent wrongTokenIntent = new Intent(intent);
        wrongTokenIntent.putExtra(WebApkReparentingHandler.EXTRA_WEBAPK_REPARENT_TOKEN, wrongToken);
        assertEquals(
                Tab.INVALID_TAB_ID,
                WebApkReparentingHandler.getInstance().getTabIdForReparenting(wrongTokenIntent));

        // The bounce with the real token still reparents the tab.
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        assertEquals(tabId, IntentHandler.getTabId(getNextStartedActivity()));
        verify(mockTab).updateAttachment(null, null);
        AsyncTabParamsManagerSingleton.getInstance().remove(tabId);
    }

    /**
     * Test that reparenting is skipped when the originating tab is closing or has navigated out of
     * scope before the WebAPK launch bounce arrives.
     */
    @Test
    public void testReparentingSkippedWhenTabClosingOrOutOfScope() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        // Case 1: Tab is closing.
        Tab closingTab = createReparentableMockTab(13);
        when(closingTab.isClosing()).thenReturn(true);
        Intent closingIntent =
                WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        closingIntent, closingTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        Robolectric.buildActivity(WebappLauncherActivity.class, closingIntent).create();
        assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(getNextStartedActivity()));
        verify(closingTab, never()).updateAttachment(any(), any());
        verify(mMockTabCreator, never()).launchNtp(anyInt());

        // Case 2: Tab navigated out of scope.
        Tab outOfScopeTab = createReparentableMockTab(14);
        when(outOfScopeTab.getUrl()).thenReturn(new GURL("https://www.example.com/out_of_scope"));
        Intent outOfScopeIntent =
                WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        outOfScopeIntent, outOfScopeTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        Robolectric.buildActivity(WebappLauncherActivity.class, outOfScopeIntent).create();
        assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(getNextStartedActivity()));
        verify(outOfScopeTab, never()).updateAttachment(any(), any());
        verify(mMockTabCreator, never()).launchNtp(anyInt());
    }

    /**
     * Test that tabs outside the main browser app (e.g. in a Custom Tab or WebAPK window) and
     * off-the-record tabs are not prepared for reparenting, so the WebAPK launches normally and the
     * tab stays in its activity.
     */
    @Test
    public void testReparentingSkippedForNonBrowserOrOffTheRecordTab() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        // Case 1: Tab outside the main browser app. isCustomTab() stays false, as it is for tabs
        // in a WebAPK window.
        Tab nonBrowserTab = createReparentableMockTab(15);
        when(nonBrowserTab.isTabInBrowser()).thenReturn(false);
        Intent nonBrowserTabIntent =
                WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        nonBrowserTabIntent, nonBrowserTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        assertFalse(
                nonBrowserTabIntent.hasExtra(WebApkReparentingHandler.EXTRA_WEBAPK_REPARENT_TOKEN));
        Robolectric.buildActivity(WebappLauncherActivity.class, nonBrowserTabIntent).create();
        assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(getNextStartedActivity()));
        verify(nonBrowserTab, never()).updateAttachment(any(), any());

        // Case 2: Off-the-record tab.
        Tab offTheRecordTab = createReparentableMockTab(16);
        when(offTheRecordTab.isOffTheRecord()).thenReturn(true);
        Intent offTheRecordIntent =
                WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        offTheRecordIntent, offTheRecordTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        assertFalse(
                offTheRecordIntent.hasExtra(WebApkReparentingHandler.EXTRA_WEBAPK_REPARENT_TOKEN));
        Robolectric.buildActivity(WebappLauncherActivity.class, offTheRecordIntent).create();
        assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(getNextStartedActivity()));
        verify(offTheRecordTab, never()).updateAttachment(any(), any());
        verify(mMockTabCreator, never()).launchNtp(anyInt());
    }

    /**
     * Test that reparenting is skipped if the originating tab can no longer be shown as is by the
     * time the WebAPK launch bounce arrives, e.g. because it was destroyed, discarded, crashed or
     * shows an error page.
     */
    @Test
    public void testReparentingSkippedWhenTabCannotBeShownAsIs() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        List<Consumer<Tab>> tabStateChanges =
                Arrays.asList(
                        tab -> when(tab.isDestroyed()).thenReturn(true),
                        tab -> when(tab.isDetachedFromActivity()).thenReturn(true),
                        tab -> when(tab.isFrozen()).thenReturn(true),
                        tab -> when(tab.needsReload()).thenReturn(true),
                        tab -> when(tab.getWebContents()).thenReturn(null),
                        tab -> when(tab.isShowingErrorPage()).thenReturn(true),
                        tab -> {
                            when(tab.isInitialized()).thenReturn(true);
                            SadTab sadTab = mock(SadTab.class);
                            when(sadTab.isShowing()).thenReturn(true);
                            SadTab.initForTesting(tab, sadTab);
                        });
        int tabId = 20;
        for (Consumer<Tab> tabStateChange : tabStateChanges) {
            Tab tab = createReparentableMockTab(tabId++);
            Intent intent =
                    WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
            WebApkReparentingHandler.getInstance()
                    .prepareIntentForReparenting(
                            intent, tab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
            tabStateChange.accept(tab);

            Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
            assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(getNextStartedActivity()));
            verify(tab, never()).updateAttachment(any(), any());
        }
        verify(mMockTabCreator, never()).launchNtp(anyInt());
    }

    /**
     * Test that pending reparenting state is cleared if the originating tab starts closing, is
     * detached from its activity or crashes before the WebAPK launch bounce arrives.
     */
    @Test
    public void testReparentingStateClearedWhenTabClosesDetachesOrCrashes() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        List<BiConsumer<TabObserver, Tab>> tabEvents =
                Arrays.asList(
                        (observer, tab) -> observer.onClosingStateChanged(tab, true),
                        (observer, tab) -> observer.onActivityAttachmentChanged(tab, null),
                        (observer, tab) -> observer.onCrash(tab));
        int tabId = 30;
        for (BiConsumer<TabObserver, Tab> tabEvent : tabEvents) {
            Tab tab = createReparentableMockTab(tabId++);
            Intent intent =
                    WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
            WebApkReparentingHandler.getInstance()
                    .prepareIntentForReparenting(
                            intent, tab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
            ArgumentCaptor<TabObserver> observerCaptor = ArgumentCaptor.forClass(TabObserver.class);
            verify(tab).addObserver(observerCaptor.capture());
            tabEvent.accept(observerCaptor.getValue(), tab);

            Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
            assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(getNextStartedActivity()));
            verify(tab, never()).updateAttachment(any(), any());
        }
        verify(mMockTabCreator, never()).launchNtp(anyInt());
    }

    /**
     * Test that if starting the WebappActivity fails, the originating tab is left untouched in its
     * window and the pending reparenting state is cleared.
     */
    @Test
    public void testReparentingAbortedWhenStartActivityFails() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        final int tabId = 40;
        Tab mockTab = createReparentableMockTab(tabId);
        when(mMockTabWindowManager.getTabModelForTab(mockTab)).thenReturn(mMockTabModel);
        when(mMockTabModel.getCount()).thenReturn(1);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        intent, mockTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        Robolectric.buildActivity(FailingStartWebappLauncherActivity.class, intent).create();

        assertFalse(AsyncTabParamsManagerSingleton.getInstance().hasParamsForTabId(tabId));
        verify(mockTab, never()).updateAttachment(any(), any());
        verify(mMockTabCreator, never()).launchNtp(anyInt());
        verify(mWebappsUtilsJni, never()).prunePreScopeNavigationHistory(any(), any());
        assertEquals(
                Tab.INVALID_TAB_ID,
                WebApkReparentingHandler.getInstance().getTabIdForReparenting(intent));
    }

    /**
     * Test that reparenting is skipped if a WebappActivity for the WebAPK is already running, since
     * it would receive the launch in onNewIntent() and never adopt the tab.
     */
    @Test
    public void testReparentingSkippedWhenWebappActivityAlreadyRunning() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        final int tabId = 41;
        Tab mockTab = createReparentableMockTab(tabId);
        BrowserServicesIntentDataProvider runningDataProvider =
                new WebApkIntentDataProviderBuilder(WEBAPK_PACKAGE_NAME, START_URL).build();
        WebappActivity runningWebappActivity =
                new WebappActivity() {
                    @Override
                    public BrowserServicesIntentDataProvider getIntentDataProvider() {
                        return runningDataProvider;
                    }
                };
        ApplicationStatus.onStateChangeForTesting(runningWebappActivity, ActivityState.CREATED);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        WebApkReparentingHandler.getInstance()
                .prepareIntentForReparenting(
                        intent, mockTab, WEBAPK_PACKAGE_NAME, START_URL, SCOPE);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getTabId(launchIntent));
        assertFalse(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, IntentHandler.EXTRA_SKIP_LOAD_ON_REPARENTING, false));
        assertFalse(AsyncTabParamsManagerSingleton.getInstance().hasParamsForTabId(tabId));
        verify(mockTab, never()).updateAttachment(any(), any());
        verify(mMockTabCreator, never()).launchNtp(anyInt());
        assertEquals(
                Tab.INVALID_TAB_ID,
                WebApkReparentingHandler.getInstance().getTabIdForReparenting(intent));
    }

    /**
     * Test that an intent with only a URL causes an intent to {@link ChromeLauncherActivity} (ie, a
     * "launch in tab" action) rather than an intent to {@link SameTaskWebApkActivity} or {@link
     * WebappActivity}. In particular, that means that this intent did NOT make it past the security
     * checks that ensure that webapp launches must be triggered from trusted intents from Chrome.
     */
    @Test
    public void testUnauthenticatedNonWebApkIntentOpensInTabAndNotWebappMode() {
        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
        intent.putExtra(WebApkConstants.EXTRA_URL, START_URL);

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());

        // And just in case, one with an empty-string WebAPK Package Name.
        intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
        intent.putExtra(WebApkConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, "");

        activity = Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    @Test
    public void testWebappLaunchWithMacVerifiedIcon() {
        String icon = "sample_base64_icon";
        byte[] mac = WebappAuthenticator.getMacForUrlAndIcon(START_URL, icon);
        assertNotNull(mac);
        String macB64 = Base64.encodeToString(mac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_1");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, icon);
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        assertEquals(WebappActivity.class.getName(), launchIntent.getComponent().getClassName());
        assertTrue(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, WebappConstants.EXTRA_IS_ICON_TRUSTED, false));
    }

    @Test
    public void testWebappLaunchWithLegacyMacDoesNotTrustIcon() {
        String icon = "sample_base64_icon";
        byte[] legacyMac = WebappAuthenticator.getMacForUrl(START_URL);
        assertNotNull(legacyMac);
        String macB64 = Base64.encodeToString(legacyMac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_2");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, icon);
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);
        intent.putExtra(WebappConstants.EXTRA_IS_ICON_TRUSTED, true);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        assertEquals(WebappActivity.class.getName(), launchIntent.getComponent().getClassName());
        assertFalse(
                IntentUtils.safeGetBooleanExtra(
                        launchIntent, WebappConstants.EXTRA_IS_ICON_TRUSTED, false));
    }

    @Test
    public void testWebappLaunchWithModifiedIconOpensInTab() {
        String icon = "sample_base64_icon";
        byte[] mac = WebappAuthenticator.getMacForUrlAndIcon(START_URL, icon);
        assertNotNull(mac);
        String macB64 = Base64.encodeToString(mac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_3");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, icon + "_modified");
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertNotNull(nextIntent);
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    @Test
    public void testWebappLaunchWithShiftedFieldBoundariesOpensInTab() {
        String icon = "PREFIX_SAMPLE_ICON";
        byte[] mac = WebappAuthenticator.getMacForUrlAndIcon(START_URL, icon);
        assertNotNull(mac);
        String macB64 = Base64.encodeToString(mac, Base64.DEFAULT);

        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_4");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL + "PREFIX_");
        intent.putExtra(WebappConstants.EXTRA_ICON, "SAMPLE_ICON");
        intent.putExtra(WebappConstants.EXTRA_MAC, macB64);

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertNotNull(nextIntent);
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    @Test
    public void testWebappLaunchWithMalformedMacOpensInTab() {
        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        intent.putExtra(WebappConstants.EXTRA_ID, "webapp_id_5");
        intent.putExtra(WebappConstants.EXTRA_URL, START_URL);
        intent.putExtra(WebappConstants.EXTRA_ICON, "sample_base64_icon");
        intent.putExtra(WebappConstants.EXTRA_MAC, "!@#$invalid_base64%$#@!");

        Activity activity =
                Robolectric.buildActivity(WebappLauncherActivity.class, intent).setup().get();

        Intent nextIntent = Shadows.shadowOf(activity).getNextStartedActivityForResult().intent;
        assertNotNull(nextIntent);
        assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                nextIntent.getComponent().getClassName());
    }

    private void registerWebApk(String webApkPackage, String startUrl) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackage, bundle, /* shareTargetMetaData= */ null);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackage, startUrl);
    }

    private Intent getNextStartedActivity() {
        return shadowOf((Application) ApplicationProvider.getApplicationContext())
                .getNextStartedActivity();
    }

    @Test
    public void testWebApkShareIntent_StashesVerifiedShareData() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Uri uri = Uri.parse("content://org.chromium.webapk.test/file.jpg");
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(uri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);
        intent.putExtra(Intent.EXTRA_SUBJECT, "title");
        intent.putExtra(Intent.EXTRA_TEXT, "text");

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertEquals("title", verifiedData.title);
        assertEquals("text", verifiedData.text);
        assertEquals(1, verifiedData.uris.size());
        assertEquals(uri, verifiedData.uris.get(0));
    }

    @Test
    public void testWebApkLaunch_StripsSpoofedVerifiedShareData() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        Bundle spoofedBundle =
                new ShareData(
                                "spoofed",
                                "spoofed",
                                Arrays.asList(Uri.parse("content://victim/secret")))
                        .toBundle();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, spoofedBundle);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        assertFalse(launchIntent.hasExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA));
    }

    @Test
    public void testWebApkShareIntent_TextOnlyShare_StashesVerifiedShareData() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        intent.putExtra(Intent.EXTRA_SUBJECT, "title");
        intent.putExtra(Intent.EXTRA_TEXT, "text");

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertEquals("title", verifiedData.title);
        assertEquals("text", verifiedData.text);
        assertTrue(verifiedData.uris == null || verifiedData.uris.isEmpty());
    }

    @Test
    public void testWebApkShareIntent_InvalidSchemeUri_FilteredOut() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Uri fileSchemeUri = Uri.parse("file:///sdcard/malicious.txt");
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(fileSchemeUri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertTrue(verifiedData.uris.isEmpty());
    }

    @Test
    public void testWebApkShareIntent_PartialFiltering_InvalidAndValidUri() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Uri fileSchemeUri = Uri.parse("file:///sdcard/malicious.txt");
        Uri validContentUri = Uri.parse("content://org.chromium.webapk.test/valid.jpg");
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.setAction(Intent.ACTION_SEND);
        intent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                "ShareActivity");
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(fileSchemeUri);
        uris.add(validContentUri);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertNotNull(launchIntent);
        Bundle verifiedBundle =
                launchIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        assertNotNull(verifiedBundle);
        ShareData verifiedData = ShareData.fromBundle(verifiedBundle);
        assertNotNull(verifiedData);
        assertEquals(1, verifiedData.uris.size());
        assertEquals(validContentUri, verifiedData.uris.get(0));
    }
}
