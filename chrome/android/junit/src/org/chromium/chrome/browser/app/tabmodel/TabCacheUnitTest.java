// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.android.util.concurrent.PausedExecutorService;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;

import java.io.File;
import java.nio.ByteBuffer;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** Unit tests for {@link TabCache} and {@link TabCacheKey}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCacheUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String CACHE_DIR_NAME = "active_tabs";

    @Mock private CipherFactory mCipherFactory;

    private final PausedExecutorService mExecutor = new PausedExecutorService();
    private TabCache mTabCache;

    @Before
    public void setUp() throws Exception {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);
        TabCache.clearGlobalState();
        mExecutor.runAll();
        setupMockCipherFactory();
    }

    @After
    public void tearDown() {
        TabCache.clearGlobalState();
        mExecutor.runAll();
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testTabCacheKey_EqualityAndProperties() {
        TabCacheKey key1 = new TabCacheKey("0", false);
        TabCacheKey key2 = new TabCacheKey("0", false);
        TabCacheKey keyIncognito = new TabCacheKey("0", true);
        TabCacheKey keyOther = new TabCacheKey("1", false);

        assertEquals(key1, key2);
        assertEquals(key1.hashCode(), key2.hashCode());
        assertNotEquals(key1, keyIncognito);
        assertNotEquals(key1, keyOther);

        assertEquals("0", key1.getTag());
        assertFalse(key1.isIncognito());
        assertTrue(keyIncognito.isIncognito());
        assertEquals("flatbufferv1_0_regular", key1.getFileName());
        assertEquals("flatbufferv1_0_incognito", keyIncognito.getFileName());
    }

    @Test
    public void testSaveTab_ValidRegular() {
        initTabCache(/* hasCipherFactory= */ false);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.isOffTheRecord()).thenReturn(false);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(1, tabState);

        TabCacheKey key = new TabCacheKey("window1", false);
        mTabCache.saveTab(key, tab);

        assertEquals(1, getSharedPreferences().getInt(key.getFileName(), Tab.INVALID_TAB_ID));

        mExecutor.runAll();
        assertTrue(getCacheFile(key.getFileName()).exists());

        LoadedTabState loaded = mTabCache.getPreLoadedTabOrLoad(key);
        assertNotNull(loaded);
        assertEquals(1, loaded.tabId);
    }

    @Test
    public void testSaveTab_ValidIncognito() {
        initTabCache(/* hasCipherFactory= */ true);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(2);
        when(tab.isOffTheRecord()).thenReturn(true);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(2, tabState);

        TabCacheKey key = new TabCacheKey("window1", true);
        mTabCache.saveTab(key, tab);

        assertEquals(2, getSharedPreferences().getInt(key.getFileName(), Tab.INVALID_TAB_ID));

        mExecutor.runAll();
        assertTrue(getCacheFile(key.getFileName()).exists());

        LoadedTabState loaded = mTabCache.getPreLoadedTabOrLoad(key);
        assertNotNull(loaded);
        assertEquals(2, loaded.tabId);
    }

    @Test
    public void testSaveTabState_Valid() {
        initTabCache(/* hasCipherFactory= */ false);
        TabCacheKey customKey = new TabCacheKey("custom_key", false);
        TabState tabState = createMockTabState();

        mTabCache.saveTabState(customKey, 42, tabState);
        assertEquals(
                42, getSharedPreferences().getInt(customKey.getFileName(), Tab.INVALID_TAB_ID));

        mExecutor.runAll();
        LoadedTabState loaded = mTabCache.getPreLoadedTabOrLoad(customKey);
        assertNotNull(loaded);
        assertEquals(42, loaded.tabId);
        assertNotNull(loaded.tabState);
    }

    @Test
    public void testPreloadTab_MultiKey() {
        initTabCache(/* hasCipherFactory= */ false);
        TabCacheKey key1 = new TabCacheKey("key_1", false);
        TabCacheKey key2 = new TabCacheKey("key_2", false);
        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();

        mTabCache.saveTabState(key1, 101, tabState1);
        mTabCache.saveTabState(key2, 102, tabState2);
        mExecutor.runAll();

        mTabCache.preloadTab(key1);
        mTabCache.preloadTab(key2);
        mExecutor.runAll();

        LoadedTabState loaded1 = mTabCache.getPreLoadedTabOrLoad(key1);
        LoadedTabState loaded2 = mTabCache.getPreLoadedTabOrLoad(key2);
        assertNotNull(loaded1);
        assertEquals(101, loaded1.tabId);
        assertNotNull(loaded2);
        assertEquals(102, loaded2.tabId);
    }

    @Test
    public void testClear() {
        initTabCache(/* hasCipherFactory= */ false);
        TabCacheKey customKey = new TabCacheKey("clear_key_test", false);
        TabState tabState = createMockTabState();

        mTabCache.saveTabState(customKey, 55, tabState);
        mExecutor.runAll();
        assertNotNull(mTabCache.getPreLoadedTabOrLoad(customKey));

        mTabCache.clear(customKey);
        assertEquals(
                Tab.INVALID_TAB_ID,
                getSharedPreferences().getInt(customKey.getFileName(), Tab.INVALID_TAB_ID));
        mExecutor.runAll();
        assertNull(mTabCache.getPreLoadedTabOrLoad(customKey));
    }

    @Test
    public void testCleanup() {
        initTabCache(/* hasCipherFactory= */ false);
        TabCacheKey customKey = new TabCacheKey("cleanup_key_test", false);
        TabState tabState = createMockTabState();

        mTabCache.saveTabState(customKey, 99, tabState);
        mExecutor.runAll();
        assertTrue(getCacheFile(customKey.getFileName()).exists());

        TabCache.cleanup(customKey);
        mExecutor.runAll();
        assertFalse(getCacheFile(customKey.getFileName()).exists());
    }

    @Test
    public void testClearGlobalState() {
        initTabCache(/* hasCipherFactory= */ false);
        TabCacheKey customKey = new TabCacheKey("global_clear_test", false);
        TabState tabState = createMockTabState();

        mTabCache.saveTabState(customKey, 77, tabState);
        mExecutor.runAll();
        assertTrue(getCacheFile(customKey.getFileName()).exists());

        TabCache.clearGlobalState();
        mExecutor.runAll();
        assertFalse(getCacheFile(customKey.getFileName()).exists());
    }

    private void initTabCache(boolean hasCipherFactory) {
        mTabCache = new TabCache(hasCipherFactory ? mCipherFactory : null);
    }

    private void setupMockCipherFactory() throws Exception {
        Cipher encryptCipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
        byte[] keyBytes = new byte[16];
        byte[] ivBytes = new byte[16];
        SecretKeySpec keySpec = new SecretKeySpec(keyBytes, "AES");
        IvParameterSpec ivSpec = new IvParameterSpec(ivBytes);
        encryptCipher.init(Cipher.ENCRYPT_MODE, keySpec, ivSpec);

        Cipher decryptCipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
        decryptCipher.init(Cipher.DECRYPT_MODE, keySpec, ivSpec);

        when(mCipherFactory.getCipher(Cipher.ENCRYPT_MODE)).thenReturn(encryptCipher);
        when(mCipherFactory.getCipher(Cipher.DECRYPT_MODE)).thenReturn(decryptCipher);
    }

    private File getCacheFile(String fileName) {
        return new File(
                ContextUtils.getApplicationContext().getDir(CACHE_DIR_NAME, Context.MODE_PRIVATE),
                fileName);
    }

    private SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(CACHE_DIR_NAME, Context.MODE_PRIVATE);
    }

    private TabState createMockTabState() {
        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        ByteBuffer buffer = ByteBuffer.allocateDirect(10);
        when(tabState.contentsState.buffer()).thenReturn(buffer);
        when(tabState.contentsState.version()).thenReturn(2);
        return tabState;
    }
}
