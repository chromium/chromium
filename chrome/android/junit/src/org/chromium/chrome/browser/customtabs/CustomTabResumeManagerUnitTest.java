// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.util.Map;

/** Unit tests for {@link CustomTabResumeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    @Mock private org.chromium.components.commerce.core.ShoppingService mShoppingService;

    private Intent mIntent;
    private CustomTabResumeManager mResumeManager;

    @Before
    public void setUp() {
        mCipherFactory = new CipherFactory();
        setUpIntentMocks();
        setUpTabModelMocks();
        setUpWindowAndroidMocks();
        setUpShoppingServiceMocks();
        setUpTabImplJniMocks();

        mResumeManager = new CustomTabResumeManager(mIntentDataProvider, mCipherFactory);
    }

    @After
    public void tearDown() {
        // Clear static cache between tests to avoid pollution.
        try {
            java.lang.reflect.Field cacheField =
                    CustomTabResumeManager.class.getDeclaredField("sInMemoryStateCache");
            cacheField.setAccessible(true);
            Map<?, ?> cache = (Map<?, ?>) cacheField.get(null);
            cache.clear();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
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
                        false,
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

        java.nio.ByteBuffer buffer = java.nio.ByteBuffer.allocateDirect(10);
        org.chromium.chrome.browser.tab.WebContentsState contentsState =
                mock(org.chromium.chrome.browser.tab.WebContentsState.class);
        when(contentsState.buffer()).thenReturn(buffer);
        when(contentsState.version()).thenReturn(2);
        state.contentsState = contentsState;

        org.chromium.chrome.browser.tab.WebContentsState.Natives webContentsStateJni =
                mock(org.chromium.chrome.browser.tab.WebContentsState.Natives.class);
        org.chromium.chrome.browser.tab.WebContentsStateJni.setInstanceForTesting(
                webContentsStateJni);

        org.chromium.chrome.browser.tab.WebContentsState.WebContentsStateMetadata metadata =
                new org.chromium.chrome.browser.tab.WebContentsState.WebContentsStateMetadata(
                        "Title", "https://example.com", false);
        when(webContentsStateJni.getMetadata(any(), anyInt())).thenReturn(metadata);

        seedInMemoryStateCache("session-123", state);

        Tab restoredTab =
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id");

        assertNotNull(restoredTab);
        assertEquals(state.timestampMillis, restoredTab.getTimestampMillis());
    }

    @Test
    public void testMaybeRestoreTab_restoreFromDisk() throws Exception {
        mIntent.putExtra(EXTRA_RESTORE_TAB, true);
        mIntent.putExtra(EXTRA_EMBEDDER_TAB_ID, "session-456");

        // Create a TabState.
        TabState state = new TabState();
        state.timestampMillis = 2000L;
        state.url = org.chromium.url.GURL.emptyGURL();

        // Seed contentsState.
        java.nio.ByteBuffer buffer = java.nio.ByteBuffer.allocateDirect(10);
        org.chromium.chrome.browser.tab.WebContentsState contentsState =
                mock(org.chromium.chrome.browser.tab.WebContentsState.class);
        when(contentsState.buffer()).thenReturn(buffer);
        when(contentsState.version()).thenReturn(2);
        state.contentsState = contentsState;

        // Mock WebContentsState JNI calls.
        org.chromium.chrome.browser.tab.WebContentsState.Natives webContentsStateJni =
                mock(org.chromium.chrome.browser.tab.WebContentsState.Natives.class);
        org.chromium.chrome.browser.tab.WebContentsStateJni.setInstanceForTesting(
                webContentsStateJni);

        org.chromium.chrome.browser.tab.WebContentsState.WebContentsStateMetadata metadata =
                new org.chromium.chrome.browser.tab.WebContentsState.WebContentsStateMetadata(
                        "Title", "https://example.com", false);
        when(webContentsStateJni.getMetadata(any(), anyInt())).thenReturn(metadata);

        // Save it to disk.
        File storageDir =
                new File(
                        androidx.test.core.app.ApplicationProvider.getApplicationContext()
                                .getFilesDir(),
                        "cct_tab_data");
        if (!storageDir.exists()) {
            storageDir.mkdirs();
        }
        int tabId = Math.max(1, "session-456".hashCode() & 0x7fffffff);
        org.chromium.chrome.browser.tabpersistence.TabStateFileManager.saveState(
                storageDir, state, tabId, /* isEncrypted= */ true, mCipherFactory);

        // Try to restore it.
        Tab restoredTab =
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id");

        assertNotNull(restoredTab);
        assertEquals(state.timestampMillis, restoredTab.getTimestampMillis());
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
        org.chromium.chrome.browser.tabmodel.TabRemover tabRemover =
                mock(org.chromium.chrome.browser.tabmodel.TabRemover.class);
        when(mTabModel.getTabRemover()).thenReturn(tabRemover);
        when(mTab.getId()).thenReturn(42);
    }

    private void setUpWindowAndroidMocks() {
        when(mWindowAndroid.getOcclusionSupplier())
                .thenReturn(org.chromium.base.supplier.ObservableSuppliers.alwaysFalse());

        java.lang.ref.WeakReference<android.content.Context> contextRef =
                new java.lang.ref.WeakReference<>(
                        androidx.test.core.app.ApplicationProvider.getApplicationContext());
        when(mWindowAndroid.getContext()).thenReturn(contextRef);
    }

    private void setUpShoppingServiceMocks() {
        org.chromium.chrome.browser.commerce.ShoppingServiceFactory.setShoppingServiceForTesting(
                mShoppingService);

        org.chromium.components.commerce.core.CommerceFeatureUtils.Natives commerceJni =
                mock(org.chromium.components.commerce.core.CommerceFeatureUtils.Natives.class);
        org.chromium.components.commerce.core.CommerceFeatureUtilsJni.setInstanceForTesting(
                commerceJni);
        when(commerceJni.isPriceAnnotationsEnabled(anyLong())).thenReturn(false);
    }

    private void setUpTabImplJniMocks() {
        // TabImpl requires a mock JNI native implementation for tests using TabBuilder.
        try {
            Class<?> nativesClass =
                    Class.forName("org.chromium.chrome.browser.tab.TabImpl$Natives");
            Class<?> tabImplClass = Class.forName("org.chromium.chrome.browser.tab.TabImpl");
            Object proxy =
                    java.lang.reflect.Proxy.newProxyInstance(
                            nativesClass.getClassLoader(),
                            new Class<?>[] {nativesClass},
                            new java.lang.reflect.InvocationHandler() {
                                @Override
                                public Object invoke(
                                        Object proxy,
                                        java.lang.reflect.Method method,
                                        Object[] args)
                                        throws Throwable {
                                    if (method.getName().equals("init")) {
                                        Object tabCaller = args[0];
                                        java.lang.reflect.Field nativeField =
                                                tabImplClass.getDeclaredField("mNativeTabAndroid");
                                        nativeField.setAccessible(true);
                                        nativeField.setLong(tabCaller, 12345L);
                                        return null;
                                    }
                                    Class<?> returnType = method.getReturnType();
                                    if (returnType.equals(boolean.class)) return false;
                                    if (returnType.equals(int.class)) return 0;
                                    if (returnType.equals(long.class)) return 0L;
                                    return null;
                                }
                            });
            Class<?> jniClass = Class.forName("org.chromium.chrome.browser.tab.TabImplJni");
            java.lang.reflect.Method setInstanceMethod =
                    jniClass.getMethod("setInstanceForTesting", nativesClass);
            setInstanceMethod.invoke(null, proxy);
        } catch (Exception e) {
            throw new RuntimeException("Failed to mock TabImpl JNI", e);
        }
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

    private void seedInMemoryStateCache(String embedderTabId, TabState state) {
        try {
            java.lang.reflect.Field cacheField =
                    CustomTabResumeManager.class.getDeclaredField("sInMemoryStateCache");
            cacheField.setAccessible(true);
            @SuppressWarnings("unchecked")
            Map<String, TabState> cache = (Map<String, TabState>) cacheField.get(null);
            cache.put(embedderTabId, state);
        } catch (Exception e) {
            throw new RuntimeException("Failed to seed memory cache", e);
        }
    }
}
