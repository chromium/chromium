// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.io.File;
import java.nio.ByteBuffer;

/**
 * Tests whether TabState can be saved and restored to disk properly. Also checks to see if
 * TabStates from previous versions of Chrome can still be loaded and upgraded.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabStateTest {
    private TestTabModelDirectory mTestTabModelDirectory;
    private CipherFactory mCipherFactory;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mCipherFactory = new CipherFactory();
        mTestTabModelDirectory =
                new TestTabModelDirectory(
                        ApplicationProvider.getApplicationContext(), "TabStateTest", null);
    }

    @After
    public void tearDown() {
        if (mTestTabModelDirectory != null) {
            mTestTabModelDirectory.tearDown();
        }
    }

    private void loadAndCheckTabState(TestTabModelDirectory.TabStateInfo info) throws Exception {
        mTestTabModelDirectory.writeTabStateFile(info);

        File tabStateFile = new File(mTestTabModelDirectory.getBaseDirectory(), info.filename);
        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(tabStateFile, false, mCipherFactory);
        Assert.assertNotNull(tabState);
        Assert.assertEquals(info.url, tabState.contentsState.getVirtualUrlFromState());
        Assert.assertEquals(info.title, tabState.contentsState.getDisplayTitleFromState());
        Assert.assertEquals(info.version, tabState.contentsState.version());
    }

    @Test
    @SmallTest
    public void testLoadV2Tabs() throws Exception {
        // Standard English tabs.
        loadAndCheckTabState(TestTabModelDirectory.V2_DUCK_DUCK_GO);
        loadAndCheckTabState(TestTabModelDirectory.V2_TEXTAREA);
        loadAndCheckTabState(TestTabModelDirectory.V2_GOOGLE_COM_FBS);
        loadAndCheckTabState(TestTabModelDirectory.V2_GOOGLE_CA_FBS);

        // Chinese characters.
        loadAndCheckTabState(TestTabModelDirectory.V2_BAIDU);

        // Hebrew, RTL.
        loadAndCheckTabState(TestTabModelDirectory.V2_HAARETZ);
    }

    @Test
    @SmallTest
    public void testWebContentsStateMetadataCaching() throws Exception {
        TestTabModelDirectory.TabStateInfo info = TestTabModelDirectory.V2_DUCK_DUCK_GO;
        mTestTabModelDirectory.writeTabStateFile(info);
        File tabStateFile = new File(mTestTabModelDirectory.getBaseDirectory(), info.filename);
        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(tabStateFile, false, mCipherFactory);
        Assert.assertNotNull(tabState);
        WebContentsState state = tabState.contentsState;
        Assert.assertNotNull(state);

        // 1. First call extracts and caches
        WebContentsState.WebContentsStateMetadata metadata1 = state.getMetadata();
        Assert.assertNotNull(metadata1);
        Assert.assertEquals(info.title, metadata1.title);
        Assert.assertEquals(info.url, metadata1.virtualUrl);

        // 2. Second call returns the exact same cached instance
        WebContentsState.WebContentsStateMetadata metadata2 = state.getMetadata();
        Assert.assertSame("Should return cached metadata instance", metadata1, metadata2);

        // 3. Swap buffer, cache must be cleared and re-evaluated
        boolean swapped =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> state.maybeSwapPackedData(state.buffer(), false));
        Assert.assertTrue(swapped);

        // 4. After swap, getMetadata() should return a NEW instance
        WebContentsState.WebContentsStateMetadata metadata3 = state.getMetadata();
        Assert.assertNotNull(metadata3);
        Assert.assertNotSame("Cache should be cleared after swap", metadata1, metadata3);
        Assert.assertEquals(info.title, metadata3.title);
    }

    @Test
    @SmallTest
    public void testWebContentsStateMetadataFallback() throws Exception {
        TestTabModelDirectory.TabStateInfo info = TestTabModelDirectory.V2_DUCK_DUCK_GO;
        mTestTabModelDirectory.writeTabStateFile(info);
        File tabStateFile = new File(mTestTabModelDirectory.getBaseDirectory(), info.filename);
        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(tabStateFile, false, mCipherFactory);
        Assert.assertNotNull(tabState);
        WebContentsState state = tabState.contentsState;
        Assert.assertNotNull(state);

        // Create a new WebContentsState with an unsupported version (e.g., 999)
        WebContentsState fallbackState = new WebContentsState(state.buffer(), 999);

        // 1. getMetadata() should return null (fallback triggered)
        Assert.assertNull(
                "Should return null for unsupported version", fallbackState.getMetadata());

        // 2. Title and URL queries should now return null as fallback JNI paths are deleted
        Assert.assertNull(fallbackState.getDisplayTitleFromState());
        Assert.assertNull(fallbackState.getVirtualUrlFromState());
    }

    @Test
    @SmallTest
    public void testWebContentsStateMetadataUrlSanitization() throws Exception {
        // We need a profile to create a single navigation state
        Profile profile =
                ThreadUtils.runOnUiThreadBlocking(() -> ProfileManager.getLastUsedRegularProfile());
        Assert.assertNotNull(profile);

        // 1. Create a serialized state with a raw NTP URL: "chrome://newtab/"
        ByteBuffer buffer =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return WebContentsStateJni.get()
                                    .createSingleNavigationStateAsByteBuffer(
                                            profile, "New Tab", "chrome://newtab/", null, 0, null);
                        });
        Assert.assertNotNull(buffer);

        WebContentsState state = new WebContentsState(buffer, 2); // Version 2

        // 2. Query virtual URL. It should be sanitized to "chrome-native://newtab/" on the fast
        // path!
        WebContentsState.WebContentsStateMetadata metadata = state.getMetadata();
        Assert.assertNotNull(metadata);
        Assert.assertEquals("chrome-native://newtab/", metadata.virtualUrl);
        Assert.assertEquals("New Tab", metadata.title);
    }
}
