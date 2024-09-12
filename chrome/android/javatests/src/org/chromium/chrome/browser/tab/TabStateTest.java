// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.io.File;

/**
 * Tests whether TabState can be saved and restored to disk properly. Also checks to see if
 * TabStates from previous versions of Chrome can still be loaded and upgraded.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabStateTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private TestTabModelDirectory mTestTabModelDirectory;
    private CipherFactory mCipherFactory;

    @Before
    public void setUp() {
        mCipherFactory = new CipherFactory();
        mTestTabModelDirectory =
                new TestTabModelDirectory(
                        ApplicationProvider.getApplicationContext(), "TabStateTest", null);
    }

    @After
    public void tearDown() {
        mTestTabModelDirectory.tearDown();
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
    public void testLoadV0Tabs() throws Exception {
        TabStateFileManager.setChannelNameOverrideForTest("stable");
        loadAndCheckTabState(TestTabModelDirectory.M18_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M18_NTP);
    }

    @Test
    @SmallTest
    public void testLoadV1Tabs() throws Exception {
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_CA);
    }

    @Test
    @SmallTest
    public void testLoadV2Tabs() throws Exception {
        // Standard English tabs.
        loadAndCheckTabState(TestTabModelDirectory.V2_DUCK_DUCK_GO);
        loadAndCheckTabState(TestTabModelDirectory.V2_TEXTAREA);

        // Chinese characters.
        loadAndCheckTabState(TestTabModelDirectory.V2_BAIDU);

        // Hebrew, RTL.
        loadAndCheckTabState(TestTabModelDirectory.V2_HAARETZ);
    }
}
