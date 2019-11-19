// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Color;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.tab.TabState.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.io.File;
import java.nio.ByteBuffer;

/**
 * Tests whether TabState can be saved and restored to disk properly. Also checks to see if
 * TabStates from previous versions of Chrome can still be loaded and upgraded.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabStateTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private TestTabModelDirectory mTestTabModelDirectory;

    @Before
    public void setUp() {
        mTestTabModelDirectory = new TestTabModelDirectory(
                InstrumentationRegistry.getTargetContext(), "TabStateTest", null);
    }

    @After
    public void tearDown() {
        TabState.setChannelNameOverrideForTest(null);
        mTestTabModelDirectory.tearDown();
    }

    private void loadAndCheckTabState(TestTabModelDirectory.TabStateInfo info) throws Exception {
        mTestTabModelDirectory.writeTabStateFile(info);

        File tabStateFile = new File(mTestTabModelDirectory.getBaseDirectory(), info.filename);
        TabState tabState = TabState.restoreTabState(tabStateFile, false);
        Assert.assertNotNull(tabState);
        Assert.assertEquals(info.url, tabState.getVirtualUrlFromState());
        Assert.assertEquals(info.title, tabState.getDisplayTitleFromState());
        Assert.assertEquals(info.version, tabState.contentsState.version());
    }

    @Test
    @SmallTest
    public void testLoadV0Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest("stable");
        loadAndCheckTabState(TestTabModelDirectory.M18_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M18_NTP);
    }

    @Test
    @SmallTest
    public void testLoadV1Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest(null);
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_CA);
    }

    @Test
    @SmallTest
    public void testLoadV2Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest(null);

        // Standard English tabs.
        loadAndCheckTabState(TestTabModelDirectory.V2_DUCK_DUCK_GO);
        loadAndCheckTabState(TestTabModelDirectory.V2_TEXTAREA);

        // Chinese characters.
        loadAndCheckTabState(TestTabModelDirectory.V2_BAIDU);

        // Hebrew, RTL.
        loadAndCheckTabState(TestTabModelDirectory.V2_HAARETZ);
    }

    @Test
    @SmallTest
    public void testSaveLoadThroughBundle() {
        TabState tabState = new TabState();
        byte[] bytes = {'A', 'B', 'C'};
        tabState.contentsState = new WebContentsState(ByteBuffer.allocateDirect(bytes.length));
        tabState.contentsState.buffer().put(bytes);
        tabState.timestampMillis = 1234;
        tabState.parentId = 2;
        tabState.openerAppId = "app";
        tabState.contentsState.setVersion(TabState.CONTENTS_STATE_CURRENT_VERSION);
        tabState.themeColor = Color.BLACK;
        tabState.mIsIncognito = true;

        Bundle b = new Bundle();
        TabState.saveState(b, tabState);
        TabState restoredState = TabState.restoreTabState(b);

        Assert.assertEquals(restoredState.contentsState.buffer(), tabState.contentsState.buffer());
        Assert.assertEquals(tabState.timestampMillis, restoredState.timestampMillis);
        Assert.assertEquals(tabState.parentId, restoredState.parentId);
        Assert.assertEquals(tabState.openerAppId, restoredState.openerAppId);
        Assert.assertEquals(tabState.timestampMillis, restoredState.timestampMillis);
        Assert.assertEquals(
                tabState.contentsState.version(), restoredState.contentsState.version());
        Assert.assertEquals(tabState.themeColor, restoredState.themeColor);
        Assert.assertEquals(tabState.mIsIncognito, restoredState.mIsIncognito);
    }
}
