// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.CustomTabResumeManager.EXTRA_EMBEDDER_TAB_ID;
import static org.chromium.chrome.browser.customtabs.CustomTabResumeManager.EXTRA_RESTORE_TAB;

import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.io.File;
import java.nio.ByteBuffer;

/** Unit tests for {@link CustomTabResumeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabResumeManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    private CipherFactory mCipherFactory;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private Tab mTab;
    @Mock private Tab mRestoredTab;

    private Intent mIntent;
    private CustomTabResumeManager mResumeManager;

    @Before
    public void setUp() {
        mCipherFactory = new CipherFactory();
        setUpIntentMocks();
        setUpTabModelMocks();

        when(mRestoredTab.getUserDataHost()).thenReturn(new UserDataHost());
        TabBuilder.setTabForTesting(mRestoredTab);

        mResumeManager = new CustomTabResumeManager(mIntentDataProvider, mCipherFactory);
    }

    @After
    public void tearDown() {
        // Clear static cache between tests to avoid pollution.
        CustomTabResumeManager.getInMemoryStateCacheForTesting().clear();
    }

    // =========================================================================
    // shouldCreateTabResumeManager tests
    // =========================================================================

    @Test
    public void testShouldCreateTabResumeManager_nullIntentData_returnsFalse() {
        assertFalse(CustomTabResumeManager.shouldCreateTabResumeManager(null));
    }

    @Test
    public void testShouldCreateTabResumeManager_missingEmbedderTabId_returnsFalse() {
        mIntent.removeExtra(EXTRA_EMBEDDER_TAB_ID);
        assertFalse(CustomTabResumeManager.shouldCreateTabResumeManager(mIntentDataProvider));
    }

    @Test
    public void testShouldCreateTabResumeManager_hasEmbedderTabId_returnsTrue() {
        mIntent.putExtra(EXTRA_EMBEDDER_TAB_ID, "test-session-id");
        assertTrue(CustomTabResumeManager.shouldCreateTabResumeManager(mIntentDataProvider));
    }

    // =========================================================================
    // isTabResumptionRequested tests
    // =========================================================================

    @Test
    public void testIsTabResumptionRequested_nullIntent_returnsFalse() {
        assertFalse(CustomTabResumeManager.isTabResumptionRequested(null));
    }

    @Test
    public void testIsTabResumptionRequested_emptyIntent_returnsFalse() {
        Intent emptyIntent = new Intent();
        assertFalse(CustomTabResumeManager.isTabResumptionRequested(emptyIntent));
    }

    @Test
    public void testIsTabResumptionRequested_hasRestoreTabOnly_returnsFalse() {
        Intent intent = createResumptionIntent(null, /* restoreTab= */ true);
        assertFalse(CustomTabResumeManager.isTabResumptionRequested(intent));
    }

    @Test
    public void testIsTabResumptionRequested_hasRestoreTabAndEmbedderTabId_returnsTrue() {
        Intent intent = createResumptionIntent("test-session-id", /* restoreTab= */ true);
        assertTrue(CustomTabResumeManager.isTabResumptionRequested(intent));
    }

    // =========================================================================
    // shouldForceRelaunchForResumption tests
    // =========================================================================

    @Test
    public void testShouldForceRelaunchForResumption_nullIntentData_returnsFalse() {
        assertFalse(
                CustomTabResumeManager.shouldForceRelaunchForResumption(null, mIntentDataProvider));
    }

    @Test
    public void testShouldForceRelaunchForResumption_resumptionRequested_returnsTrue() {
        Intent newIntent = createResumptionIntent("test-session-id", /* restoreTab= */ true);
        BrowserServicesIntentDataProvider newProvider = mockIntentProvider(newIntent);

        assertTrue(CustomTabResumeManager.shouldForceRelaunchForResumption(newProvider, null));
    }

    @Test
    public void
            testShouldForceRelaunchForResumption_noResumptionRequested_nullCurrentIntent_returnsFalse() {
        Intent newIntent = new Intent();
        BrowserServicesIntentDataProvider newProvider = mockIntentProvider(newIntent);

        assertFalse(CustomTabResumeManager.shouldForceRelaunchForResumption(newProvider, null));
    }

    @Test
    public void testShouldForceRelaunchForResumption_differentSessionIds_returnsTrue() {
        Intent newIntent = createResumptionIntent("session-b", /* restoreTab= */ false);
        BrowserServicesIntentDataProvider newProvider = mockIntentProvider(newIntent);

        Intent currentIntent = createResumptionIntent("session-a", /* restoreTab= */ false);
        BrowserServicesIntentDataProvider currentProvider = mockIntentProvider(currentIntent);

        assertTrue(
                CustomTabResumeManager.shouldForceRelaunchForResumption(
                        newProvider, currentProvider));
    }

    @Test
    public void testShouldForceRelaunchForResumption_sameSessionIds_returnsFalse() {
        Intent newIntent = createResumptionIntent("session-a", /* restoreTab= */ false);
        BrowserServicesIntentDataProvider newProvider = mockIntentProvider(newIntent);

        Intent currentIntent = createResumptionIntent("session-a", /* restoreTab= */ false);
        BrowserServicesIntentDataProvider currentProvider = mockIntentProvider(currentIntent);

        assertFalse(
                CustomTabResumeManager.shouldForceRelaunchForResumption(
                        newProvider, currentProvider));
    }

    // =========================================================================
    // maybeRestoreTab tests
    // =========================================================================

    @Test
    public void testMaybeRestoreTab_noResumptionRequested_returnsNull() {
        mIntent.putExtra(EXTRA_RESTORE_TAB, false);
        assertNull(
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        /* isOffTheRecord= */ false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id"));
    }

    @Test
    public void testMaybeRestoreTab_cacheHit() {
        mIntent.putExtra(EXTRA_RESTORE_TAB, true);
        mIntent.putExtra(EXTRA_EMBEDDER_TAB_ID, "session-123");

        // Seed the static cache.
        TabState state = new TabState();
        state.timestampMillis = 1000L;
        state.contentsState =
                new WebContentsState(
                        ByteBuffer.allocateDirect(10),
                        WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        CustomTabResumeManager.getInMemoryStateCacheForTesting().put("session-123", state);

        Tab restoredTab =
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        /* isOffTheRecord= */ false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id");

        assertSame(mRestoredTab, restoredTab);
        assertEquals("app-id", TabAssociatedApp.getAppId(restoredTab));
    }

    @Test
    public void testMaybeRestoreTab_restoreFromDisk() throws Exception {
        mIntent.putExtra(EXTRA_RESTORE_TAB, true);
        mIntent.putExtra(EXTRA_EMBEDDER_TAB_ID, "session-456");

        // Create a TabState.
        TabState state = new TabState();
        state.timestampMillis = 2000L;
        state.url = GURL.emptyGURL();
        state.contentsState =
                new WebContentsState(
                        ByteBuffer.allocateDirect(10),
                        WebContentsState.CONTENTS_STATE_CURRENT_VERSION);

        // Save it to disk.
        File storageDir =
                new File(ContextUtils.getApplicationContext().getFilesDir(), "cct_tab_data");
        if (!storageDir.exists()) {
            storageDir.mkdirs();
        }
        int tabId = Math.max(1, "session-456".hashCode() & 0x7fffffff);
        TabStateFileManager.saveState(
                storageDir, state, tabId, /* isEncrypted= */ true, mCipherFactory);

        // Try to restore it.
        Tab restoredTab =
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        /* isOffTheRecord= */ false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id");

        assertSame(mRestoredTab, restoredTab);
        assertEquals("app-id", TabAssociatedApp.getAppId(restoredTab));
        assertEquals(
                state.timestampMillis,
                CustomTabResumeManager.getInMemoryStateCacheForTesting()
                        .get("session-456")
                        .timestampMillis);
    }

    // =========================================================================
    // registerTabIfResumptionEnabled tests
    // =========================================================================

    @Test
    public void testRegisterTabIfResumptionEnabled() {
        mIntent.putExtra(EXTRA_EMBEDDER_TAB_ID, "session-123");

        mResumeManager.registerTabIfResumptionEnabled(mTab);

        verify(mTab).addObserver(any());
    }

    @Test
    public void testDestroy_removesObserverFromRegisteredTabs() {
        mIntent.putExtra(EXTRA_EMBEDDER_TAB_ID, "session-123");

        mResumeManager.registerTabIfResumptionEnabled(mTab);
        mResumeManager.destroy();

        verify(mTab).removeObserver(any());
    }

    // =========================================================================
    // Private Setup & Mocking Helpers
    // =========================================================================

    private void setUpIntentMocks() {
        mIntent = new Intent();
        when(mIntentDataProvider.getIntent()).thenReturn(mIntent);
    }

    private void setUpTabModelMocks() {
        when(mTabModelSelector.getModel(any(Boolean.class))).thenReturn(mTabModel);
        TabRemover tabRemover = mock(TabRemover.class);
        when(mTabModel.getTabRemover()).thenReturn(tabRemover);
        when(mTab.getId()).thenReturn(42);
    }

    private Intent createResumptionIntent(String embedderTabId, boolean restoreTab) {
        Intent intent = new Intent();
        if (embedderTabId != null) {
            intent.putExtra(EXTRA_EMBEDDER_TAB_ID, embedderTabId);
        }
        if (restoreTab) {
            intent.putExtra(EXTRA_RESTORE_TAB, true);
        }
        return intent;
    }

    private BrowserServicesIntentDataProvider mockIntentProvider(Intent intent) {
        BrowserServicesIntentDataProvider provider = mock(BrowserServicesIntentDataProvider.class);
        when(provider.getIntent()).thenReturn(intent);
        return provider;
    }
}
