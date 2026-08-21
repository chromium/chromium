// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.io.File;
import java.nio.ByteBuffer;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** Unit tests for {@link ActiveTabCache}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActiveTabCacheUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String WINDOW_TAG = "window_tag";
    private static final String CACHE_DIR_NAME = "active_tabs";
    private static final String FLATBUFFER_PREFIX = "flatbufferv1_";
    private static final String REGULAR_SUFFIX = "_regular";
    private static final String INCOGNITO_SUFFIX = "_incognito";

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private CipherFactory mCipherFactory;

    private final PausedExecutorService mExecutor = new PausedExecutorService();
    private ActiveTabCache mActiveTabCache;

    @Before
    public void setUp() throws Exception {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);
        TabCacheManager.resetForTesting();
        // Clear global state to ensure a clean start.
        ActiveTabCache.clearGlobalState();
        // Run the clear global state task.
        mExecutor.runAll();
        setupMockCipherFactory();
    }

    @After
    public void tearDown() {
        TabCacheManager.resetForTesting();
        ActiveTabCache.clearGlobalState();
        mExecutor.runAll();
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testSaveActiveTab_ValidRegular() {
        initActiveTabCache(/* hasCipherFactory= */ false);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.isOffTheRecord()).thenReturn(false);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(1, tabState);

        mActiveTabCache.saveActiveTab(tab);

        // Assert pref is set synchronously
        String fileName = getFileName(false);
        assertEquals(1, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        // Run the save task
        mExecutor.runAll();

        // Assert file is created
        assertTrue(getCacheFile(false).exists());
    }

    @Test
    public void testSaveActiveTab_ValidIncognito() {
        initActiveTabCache(/* hasCipherFactory= */ true);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(2);
        when(tab.isOffTheRecord()).thenReturn(true);

        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(2, tabState);

        mActiveTabCache.saveActiveTab(tab);

        String fileName = getFileName(true);
        assertEquals(2, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        mExecutor.runAll();
        assertTrue(getCacheFile(true).exists());
    }

    @Test
    public void testSaveActiveTab_NullTabState_DeletesStaleRegular() {
        initActiveTabCache(/* hasCipherFactory= */ false);

        // Pre-populate cache with a valid tab first to simulate stale state.
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab1.isOffTheRecord()).thenReturn(false);

        TabState tabState1 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(1, tabState1);

        mActiveTabCache.saveActiveTab(tab1);
        mExecutor.runAll();
        assertTrue(getCacheFile(false).exists());
        String fileName = getFileName(false);
        assertEquals(1, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        // Now save tab with null tabState.
        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.isOffTheRecord()).thenReturn(false);
        TabStateExtractor.setTabStateForTesting(2, null);

        mActiveTabCache.saveActiveTab(tab2);

        // Assert pref is removed synchronously.
        assertEquals(
                Tab.INVALID_TAB_ID, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        // Run deletion tasks.
        mExecutor.runAll();

        // Assert file is deleted.
        assertFalse(getCacheFile(false).exists());
    }

    @Test
    public void testSaveActiveTab_NullContentsState_DeletesStaleRegular() {
        initActiveTabCache(/* hasCipherFactory= */ false);

        // Pre-populate cache.
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab1.isOffTheRecord()).thenReturn(false);

        TabState tabState1 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(1, tabState1);

        mActiveTabCache.saveActiveTab(tab1);
        mExecutor.runAll();
        assertTrue(getCacheFile(false).exists());
        String fileName = getFileName(false);
        assertEquals(1, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        // Now save tab with tabState having null contentsState.
        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.isOffTheRecord()).thenReturn(false);

        TabState tabState2 = new TabState();
        tabState2.contentsState = null;
        TabStateExtractor.setTabStateForTesting(2, tabState2);

        mActiveTabCache.saveActiveTab(tab2);

        // Assert pref is removed.
        assertEquals(
                Tab.INVALID_TAB_ID, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        mExecutor.runAll();
        assertFalse(getCacheFile(false).exists());
    }

    @Test
    public void testSaveActiveTab_NullTabState_DeletesStaleIncognito() {
        initActiveTabCache(/* hasCipherFactory= */ true);

        // Pre-populate cache.
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab1.isOffTheRecord()).thenReturn(true);

        TabState tabState1 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(1, tabState1);

        mActiveTabCache.saveActiveTab(tab1);
        mExecutor.runAll();
        assertTrue(getCacheFile(true).exists());
        String fileName = getFileName(true);
        assertEquals(1, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        // Save tab with null tabState.
        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.isOffTheRecord()).thenReturn(true);
        TabStateExtractor.setTabStateForTesting(2, null);

        mActiveTabCache.saveActiveTab(tab2);

        assertEquals(
                Tab.INVALID_TAB_ID, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));
        mExecutor.runAll();
        assertFalse(getCacheFile(true).exists());
    }

    @Test
    public void testSaveActiveTab_NullContentsState_DeletesStaleIncognito() {
        initActiveTabCache(/* hasCipherFactory= */ true);

        // Pre-populate cache.
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab1.isOffTheRecord()).thenReturn(true);

        TabState tabState1 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(1, tabState1);

        mActiveTabCache.saveActiveTab(tab1);
        mExecutor.runAll();
        assertTrue(getCacheFile(true).exists());
        String fileName = getFileName(true);
        assertEquals(1, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));

        // Save tab with null contentsState.
        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.isOffTheRecord()).thenReturn(true);

        TabState tabState2 = new TabState();
        tabState2.contentsState = null;
        TabStateExtractor.setTabStateForTesting(2, tabState2);

        mActiveTabCache.saveActiveTab(tab2);

        assertEquals(
                Tab.INVALID_TAB_ID, getSharedPreferences().getInt(fileName, Tab.INVALID_TAB_ID));
        mExecutor.runAll();
        assertFalse(getCacheFile(true).exists());
    }

    private void initActiveTabCache(boolean hasCipherFactory) {
        mActiveTabCache =
                new ActiveTabCache(
                        WINDOW_TAG, mTabModelSelector, hasCipherFactory ? mCipherFactory : null);
        // Run the restoration tasks queued in constructor.
        mExecutor.runAll();
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

    private File getCacheFile(boolean incognito) {
        String suffix = incognito ? INCOGNITO_SUFFIX : REGULAR_SUFFIX;
        String fileName = FLATBUFFER_PREFIX + WINDOW_TAG + suffix;
        return new File(
                ContextUtils.getApplicationContext().getDir(CACHE_DIR_NAME, Context.MODE_PRIVATE),
                fileName);
    }

    private SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(CACHE_DIR_NAME, Context.MODE_PRIVATE);
    }

    private String getFileName(boolean incognito) {
        String suffix = incognito ? INCOGNITO_SUFFIX : REGULAR_SUFFIX;
        return FLATBUFFER_PREFIX + WINDOW_TAG + suffix;
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
