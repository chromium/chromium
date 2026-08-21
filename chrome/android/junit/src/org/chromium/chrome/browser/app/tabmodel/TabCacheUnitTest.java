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

/** Unit tests for {@link TabCache}, {@link TabCacheManager}, and {@link TabCacheKey}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCacheUnitTest {
    private static final String CACHE_DIR_NAME = ActiveTabCache.CACHE_TAG;
    private static final String CUSTOM_DIR_A = "custom_dir_a";
    private static final String CUSTOM_DIR_B = "custom_dir_b";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CipherFactory mCipherFactory;

    private final PausedExecutorService mExecutor = new PausedExecutorService();
    private TabCache mTabCache;

    @Before
    public void setUp() throws Exception {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);
        TabCacheManager.resetForTesting();
        TabCache.clearGlobalState(CACHE_DIR_NAME);
        TabCache.clearGlobalState(CUSTOM_DIR_A);
        TabCache.clearGlobalState(CUSTOM_DIR_B);
        mExecutor.runAll();
        setupMockCipherFactory();
    }

    @After
    public void tearDown() {
        TabCacheManager.resetForTesting();
        TabCache.clearGlobalState(CACHE_DIR_NAME);
        TabCache.clearGlobalState(CUSTOM_DIR_A);
        TabCache.clearGlobalState(CUSTOM_DIR_B);
        mExecutor.runAll();
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testTabCacheKeyFileNames() {
        TabCacheKey keyRegular = new TabCacheKey("0", /* isIncognito= */ false);
        assertEquals("flatbufferv1_0_regular", keyRegular.getFileName());

        TabCacheKey keyIncognito = new TabCacheKey("0", /* isIncognito= */ true);
        assertEquals("flatbufferv1_0_incognito", keyIncognito.getFileName());
    }

    @Test
    public void testTabCacheManagerCreate() {
        TabCache cache = TabCacheManager.create(CUSTOM_DIR_A, mCipherFactory);
        assertNotNull(cache);
        assertEquals(CUSTOM_DIR_A, cache.getTag());
    }

    @Test
    public void testSaveTab_ValidRegular() {
        initTabCache(/* hasCipherFactory= */ false);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(10);
        when(tab.isOffTheRecord()).thenReturn(false);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(10, tabState);

        TabCacheKey key = new TabCacheKey("0", /* isIncognito= */ false);
        mTabCache.saveTab(key, tab);

        mExecutor.runAll();
        assertTrue(getCacheFile(key.getFileName()).exists());
    }

    @Test
    public void testSaveTab_ValidIncognito() {
        initTabCache(/* hasCipherFactory= */ true);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(20);
        when(tab.isOffTheRecord()).thenReturn(true);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(20, tabState);

        TabCacheKey key = new TabCacheKey("0", /* isIncognito= */ true);
        mTabCache.saveTab(key, tab);

        mExecutor.runAll();
        assertTrue(getCacheFile(key.getFileName()).exists());
    }

    @Test
    public void testSaveTab_NullStateDeletesFile() {
        initTabCache(/* hasCipherFactory= */ false);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(10);
        when(tab.isOffTheRecord()).thenReturn(false);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(10, tabState);

        TabCacheKey key = new TabCacheKey("0", /* isIncognito= */ false);
        mTabCache.saveTab(key, tab);
        mExecutor.runAll();
        assertTrue(getCacheFile(key.getFileName()).exists());

        TabStateExtractor.resetTabStatesForTesting();
        mTabCache.saveTab(key, tab);
        mExecutor.runAll();
        assertFalse(getCacheFile(key.getFileName()).exists());
    }

    @Test
    public void testPreloadAndGetPreLoadedTabOrLoad() {
        initTabCache(/* hasCipherFactory= */ false);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(10);
        when(tab.isOffTheRecord()).thenReturn(false);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(10, tabState);

        TabCacheKey key = new TabCacheKey("0", /* isIncognito= */ false);
        mTabCache.saveTab(key, tab);
        mExecutor.runAll();

        mTabCache.preloadTab(key);
        mExecutor.runAll();

        LoadedTabState loaded = mTabCache.getPreLoadedTabOrLoad(key);
        assertNotNull(loaded);
        assertEquals(10, loaded.tabId);
    }

    @Test
    public void testClearKey() {
        initTabCache(/* hasCipherFactory= */ false);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(10);
        when(tab.isOffTheRecord()).thenReturn(false);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(10, tabState);

        TabCacheKey key = new TabCacheKey("0", /* isIncognito= */ false);
        mTabCache.saveTab(key, tab);
        mExecutor.runAll();
        assertTrue(getCacheFile(key.getFileName()).exists());

        mTabCache.clear(key);
        mExecutor.runAll();
        assertFalse(getCacheFile(key.getFileName()).exists());
    }

    @Test
    public void testCleanup() {
        TabCacheKey customKey = new TabCacheKey("cleanup_test", /* isIncognito= */ false);
        TabState tabState = createMockTabState();

        initTabCache(/* hasCipherFactory= */ false);
        mTabCache.saveTabState(customKey, 42, tabState);
        mExecutor.runAll();
        assertTrue(getCacheFile(customKey.getFileName()).exists());

        TabCache.cleanup(CACHE_DIR_NAME, customKey);
        mExecutor.runAll();
        assertFalse(getCacheFile(customKey.getFileName()).exists());
    }

    @Test
    public void testClearGlobalState() {
        TabCacheKey customKey = new TabCacheKey("clear_global_test", /* isIncognito= */ false);
        TabState tabState = createMockTabState();

        initTabCache(/* hasCipherFactory= */ false);
        mTabCache.saveTabState(customKey, 42, tabState);
        mExecutor.runAll();
        assertTrue(getCacheFile(customKey.getFileName()).exists());

        TabCache.clearGlobalState(CACHE_DIR_NAME);
        mExecutor.runAll();
        assertFalse(getCacheFile(customKey.getFileName()).exists());
    }

    @Test
    public void testCustomDirectoryConstructionAndIsolation() {
        TabCache cacheA = TabCacheManager.create(CUSTOM_DIR_A, /* cipherFactory= */ null);
        TabCache cacheB = TabCacheManager.create(CUSTOM_DIR_B, /* cipherFactory= */ null);

        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(10);
        when(tab1.isOffTheRecord()).thenReturn(false);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(20);
        when(tab2.isOffTheRecord()).thenReturn(false);

        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(10, tabState1);
        TabStateExtractor.setTabStateForTesting(20, tabState2);

        TabCacheKey key = new TabCacheKey("shared_key", /* isIncognito= */ false);
        cacheA.saveTab(key, tab1);
        cacheB.saveTab(key, tab2);

        mExecutor.runAll();

        File fileA =
                new File(
                        ContextUtils.getApplicationContext()
                                .getDir(CUSTOM_DIR_A, Context.MODE_PRIVATE),
                        key.getFileName());
        File fileB =
                new File(
                        ContextUtils.getApplicationContext()
                                .getDir(CUSTOM_DIR_B, Context.MODE_PRIVATE),
                        key.getFileName());

        assertTrue(fileA.exists());
        assertTrue(fileB.exists());

        LoadedTabState loadedA = cacheA.getPreLoadedTabOrLoad(key);
        LoadedTabState loadedB = cacheB.getPreLoadedTabOrLoad(key);

        assertNotNull(loadedA);
        assertEquals(10, loadedA.tabId);
        assertNotNull(loadedB);
        assertEquals(20, loadedB.tabId);
    }

    @Test
    public void testClearAllWipesOnlyTargetDirectory() {
        TabCache cacheA = TabCacheManager.create(CUSTOM_DIR_A, /* cipherFactory= */ null);
        TabCache cacheB = TabCacheManager.create(CUSTOM_DIR_B, /* cipherFactory= */ null);

        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(10);
        when(tab1.isOffTheRecord()).thenReturn(false);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(20);
        when(tab2.isOffTheRecord()).thenReturn(false);

        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(10, tabState1);
        TabStateExtractor.setTabStateForTesting(20, tabState2);

        TabCacheKey key = new TabCacheKey("target_key", /* isIncognito= */ false);
        cacheA.saveTab(key, tab1);
        cacheB.saveTab(key, tab2);

        mExecutor.runAll();

        cacheA.clearAll();
        mExecutor.runAll();

        File fileA =
                new File(
                        ContextUtils.getApplicationContext()
                                .getDir(CUSTOM_DIR_A, Context.MODE_PRIVATE),
                        key.getFileName());
        File fileB =
                new File(
                        ContextUtils.getApplicationContext()
                                .getDir(CUSTOM_DIR_B, Context.MODE_PRIVATE),
                        key.getFileName());

        assertFalse(fileA.exists());
        assertTrue(fileB.exists());

        assertNull(cacheA.getPreLoadedTabOrLoad(key));
        LoadedTabState loadedB = cacheB.getPreLoadedTabOrLoad(key);
        assertNotNull(loadedB);
        assertEquals(20, loadedB.tabId);
    }

    @Test
    public void testStaticCleanupWithCustomDirectory() {
        TabCache cacheA = TabCacheManager.create(CUSTOM_DIR_A, /* cipherFactory= */ null);
        TabCacheKey customKey = new TabCacheKey("cleanup_custom_test", /* isIncognito= */ false);
        TabState tabState = createMockTabState();

        cacheA.saveTabState(customKey, 88, tabState);
        mExecutor.runAll();

        File fileA =
                new File(
                        ContextUtils.getApplicationContext()
                                .getDir(CUSTOM_DIR_A, Context.MODE_PRIVATE),
                        customKey.getFileName());
        assertTrue(fileA.exists());

        TabCache.cleanup(CUSTOM_DIR_A, customKey);
        mExecutor.runAll();
        assertFalse(fileA.exists());
    }

    @Test
    public void testSameTagSharesSameDirScope() {
        TabCache cache1 = TabCacheManager.create(CUSTOM_DIR_A, /* cipherFactory= */ null);
        TabCache cache2 = TabCacheManager.create(CUSTOM_DIR_A, /* cipherFactory= */ null);
        assertEquals(cache1.getDirScope(), cache2.getDirScope());

        TabCache cacheOther = TabCacheManager.create(CUSTOM_DIR_B, /* cipherFactory= */ null);
        assertNotEquals(cache1.getDirScope(), cacheOther.getDirScope());
    }

    @Test
    public void testResetForTesting() {
        TabCache cacheA = TabCacheManager.create(CUSTOM_DIR_A, /* cipherFactory= */ null);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(10);
        when(tab.isOffTheRecord()).thenReturn(false);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(10, tabState);
        TabCacheKey key = new TabCacheKey("reset_key", /* isIncognito= */ false);
        cacheA.saveTab(key, tab);
        mExecutor.runAll();

        File fileA =
                new File(
                        ContextUtils.getApplicationContext()
                                .getDir(CUSTOM_DIR_A, Context.MODE_PRIVATE),
                        key.getFileName());
        assertTrue(fileA.exists());

        TabCacheManager.resetForTesting();
        mExecutor.runAll();

        assertFalse(fileA.exists());
    }

    private void initTabCache(boolean hasCipherFactory) {
        mTabCache =
                TabCacheManager.create(CACHE_DIR_NAME, hasCipherFactory ? mCipherFactory : null);
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

    private TabState createMockTabState() {
        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        ByteBuffer buffer = ByteBuffer.allocateDirect(10);
        when(tabState.contentsState.buffer()).thenReturn(buffer);
        when(tabState.contentsState.version()).thenReturn(2);
        return tabState;
    }

    private File getCacheFile(String fileName) {
        File dir =
                ContextUtils.getApplicationContext().getDir(CACHE_DIR_NAME, Context.MODE_PRIVATE);
        return new File(dir, fileName);
    }
}
