// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.FAKE_CLASS_LOADER_PROVIDER;
import static org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.FAKE_MODULE_COMPONENT_NAME;
import static org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.FAKE_MODULE_DEX_ASSET_NAME;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.FakeDexInputStreamProvider;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link ModuleLoader}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabsDynamicModuleLoaderTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private FakeDexInputStreamProvider mDexInputStreamProvider;
    private ModuleLoader mModuleLoaderFromApk;
    private ModuleLoader mModuleLoaderFromDex;
    private ModuleLoader mModuleLoaderFromDex2;

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        mDexInputStreamProvider = new FakeDexInputStreamProvider();
        mModuleLoaderFromApk = new ModuleLoader(FAKE_MODULE_COMPONENT_NAME,
                /* dexAssetName = */ null, mDexInputStreamProvider, FAKE_CLASS_LOADER_PROVIDER);
        mModuleLoaderFromDex = new ModuleLoader(FAKE_MODULE_COMPONENT_NAME,
                FAKE_MODULE_DEX_ASSET_NAME, mDexInputStreamProvider, FAKE_CLASS_LOADER_PROVIDER);
        mModuleLoaderFromDex2 = new ModuleLoader(FAKE_MODULE_COMPONENT_NAME,
                FAKE_MODULE_DEX_ASSET_NAME, mDexInputStreamProvider, FAKE_CLASS_LOADER_PROVIDER);
    }

    @After
    public void tearDown() {
        mModuleLoaderFromApk.cleanUpLocalDex();
        mModuleLoaderFromDex.cleanUpLocalDex();
        mModuleLoaderFromDex2.cleanUpLocalDex();
    }

    /**
     * Test fake dynamic module is correctly loaded when there is no dex resource ID.
     */
    @Test
    @SmallTest
    public void testModuleLoadingFromApk_loadsModuleEntryPoint() throws TimeoutException {
        CallbackHelper onLoaded = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromApk.loadModule();

            mModuleLoaderFromApk.addCallbackAndIncrementUseCount(result -> {
                assertNotNull(result);
                onLoaded.notifyCalled();
            });
        });

        onLoaded.waitForFirst();
    }

    /**
     * Test no dex file is copied to disk when there is no dex resource ID.
     */
    @Test
    @SmallTest
    public void testModuleLoadingFromApk_doesNotCopyDexToDisk() throws TimeoutException {
        CallbackHelper onLoaded = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromApk.loadModule();

            mModuleLoaderFromApk.addCallbackAndIncrementUseCount(result -> {
                assertNotNull(result);
                onLoaded.notifyCalled();
            });
        });

        onLoaded.waitForFirst();

        assertEquals(0, mDexInputStreamProvider.getCallCount());
        assertEquals(0, mModuleLoaderFromApk.getDexDirectory().listFiles().length);
        assertFalse(mModuleLoaderFromApk.getDexFile().exists());
    }

    /**
     * Test fake dynamic module is correctly loaded with dex resource ID.
     */
    @Test
    @SmallTest
    public void testModuleLoadingFromDex_loadsModuleEntryPoint() throws TimeoutException {
        CallbackHelper onLoaded = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromDex.loadModule();

            mModuleLoaderFromDex.addCallbackAndIncrementUseCount(result -> {
                assertNotNull(result);
                onLoaded.notifyCalled();
            });
        });

        onLoaded.waitForFirst();
    }

    @Test
    @SmallTest
    public void testModuleLoadingFromDex_hasNoLocalDex_copiesDexToDisk() throws TimeoutException {
        CallbackHelper onLoaded = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromDex.loadModule();

            mModuleLoaderFromDex.addCallbackAndIncrementUseCount(result -> {
                assertNotNull(result);
                onLoaded.notifyCalled();
            });
        });

        onLoaded.waitForFirst();

        assertEquals(1, mDexInputStreamProvider.getCallCount());

        long lastUpdateTime = ContextUtils.getAppSharedPreferences().getLong(
                mModuleLoaderFromDex.getDexLastUpdateTimePrefName(), -1);
        assertEquals(mModuleLoaderFromDex.getModuleLastUpdateTime(), lastUpdateTime);

        assertNotEquals(0, mModuleLoaderFromDex.getDexDirectory().listFiles().length);
        assertTrue(mModuleLoaderFromDex.getDexFile().exists());
    }

    @Test
    @SmallTest
    public void testModuleLoadingFromDex_localDexHasSameUpdateTime_doesNotCopyDexToDisk()
            throws TimeoutException {
        CallbackHelper onLoaded1 = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromDex.loadModule();

            mModuleLoaderFromDex.addCallbackAndIncrementUseCount(
                    result -> onLoaded1.notifyCalled());
        });

        onLoaded1.waitForFirst();

        CallbackHelper onLoaded2 = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromDex2.loadModule();

            mModuleLoaderFromDex2.addCallbackAndIncrementUseCount(
                    result -> onLoaded2.notifyCalled());
        });

        onLoaded2.waitForFirst();

        assertEquals(1, mDexInputStreamProvider.getCallCount());

        long lastUpdateTime = ContextUtils.getAppSharedPreferences().getLong(
                mModuleLoaderFromDex2.getDexLastUpdateTimePrefName(), -1);
        assertEquals(mModuleLoaderFromDex2.getModuleLastUpdateTime(), lastUpdateTime);

        assertNotEquals(0, mModuleLoaderFromDex2.getDexDirectory().listFiles().length);
        assertTrue(mModuleLoaderFromDex2.getDexFile().exists());
    }

    @Test
    @SmallTest
    public void testModuleLoadingFromDex_localDexHasDifferentUpdateTime_copiesDexToDisk()
            throws TimeoutException {
        CallbackHelper onLoaded = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromDex.loadModule();

            mModuleLoaderFromDex.addCallbackAndIncrementUseCount(result -> {
                assertNotNull(result);
                onLoaded.notifyCalled();
            });
        });

        onLoaded.waitForFirst();

        assertEquals(1, mDexInputStreamProvider.getCallCount());

        long lastUpdateTime = ContextUtils.getAppSharedPreferences().getLong(
                mModuleLoaderFromDex.getDexLastUpdateTimePrefName(), -1);
        assertEquals(mModuleLoaderFromDex.getModuleLastUpdateTime(), lastUpdateTime);

        assertNotEquals(0, mModuleLoaderFromDex.getDexDirectory().listFiles().length);
        assertTrue(mModuleLoaderFromDex.getDexFile().exists());
    }

    @Test
    @SmallTest
    public void testModuleLoadingFromDex_reloadingWithoutDex_cleansUpLocalDex()
            throws TimeoutException {
        CallbackHelper onLoadedWithDex = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromDex.loadModule();

            mModuleLoaderFromDex.addCallbackAndIncrementUseCount(
                    result -> onLoadedWithDex.notifyCalled());
        });

        onLoadedWithDex.waitForFirst();
        CallbackHelper onLoadedWithoutDex = new CallbackHelper();

        runOnUiThreadBlocking(() -> {
            mModuleLoaderFromApk.loadModule();

            mModuleLoaderFromApk.addCallbackAndIncrementUseCount(
                    result -> onLoadedWithoutDex.notifyCalled());
        });

        onLoadedWithoutDex.waitForFirst();

        assertEquals(1, mDexInputStreamProvider.getCallCount());
        assertFalse(ContextUtils.getAppSharedPreferences().contains(
                mModuleLoaderFromApk.getDexLastUpdateTimePrefName()));
        assertEquals(0, mModuleLoaderFromApk.getDexDirectory().listFiles().length);
        assertFalse(mModuleLoaderFromApk.getDexFile().exists());
    }

    /**
     * Test the ModuleLoader correctly update module usage counter.
     */
    @Test
    @SmallTest
    public void testModuleUseCounter() throws TimeoutException {
        final int callbacksNumber = 3;
        CallbackHelper onLoaded = new CallbackHelper();
        List<Callback<ModuleEntryPoint>> unusedCallbacks = new ArrayList<>();
        List<Callback<ModuleEntryPoint>> callbacks = new ArrayList<>();

        runOnUiThreadBlocking(() -> {
            // Test we correctly unregister callbacks which were never notified.
            for (int i = 0; i < callbacksNumber; i++) {
                unusedCallbacks.add(result -> {});
                mModuleLoaderFromApk.addCallbackAndIncrementUseCount(unusedCallbacks.get(i));
            }
            // module has not been loaded, therefore there is no usage
            assertEquals(0, mModuleLoaderFromApk.getModuleUseCount());

            // unregister all callbacks so they should not increment module usage
            for (int i = 0; i < callbacksNumber; i++) {
                mModuleLoaderFromApk.removeCallbackAndDecrementUseCount(unusedCallbacks.get(i));
            }

            assertEquals(0, mModuleLoaderFromApk.getModuleUseCount());

            mModuleLoaderFromApk.loadModule();

            // register callbacks and wait for the notification when module is loaded
            for (int i = 0; i < callbacksNumber; i++) {
                callbacks.add(result -> onLoaded.notifyCalled());
                mModuleLoaderFromApk.addCallbackAndIncrementUseCount(callbacks.get(i));
            }
        });

        onLoaded.waitForCallback(0, callbacksNumber);

        runOnUiThreadBlocking(() -> {
            assertEquals(callbacksNumber, mModuleLoaderFromApk.getModuleUseCount());

            // unregister already notified callbacks
            for (int i = 0; i < callbacksNumber; i++) {
                mModuleLoaderFromApk.removeCallbackAndDecrementUseCount(callbacks.get(i));
            }
            assertEquals(0, mModuleLoaderFromApk.getModuleUseCount());
        });
    }
}
