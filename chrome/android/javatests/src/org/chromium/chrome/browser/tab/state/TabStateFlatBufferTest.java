// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ByteBufferTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.io.IOException;
import java.util.Locale;
import java.util.concurrent.ExecutionException;

/** Test relating to FlatBuffer portion of {@link TabStateFileManager} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabStateFlatBufferTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private static EmbeddedTestServer sTestServer;

    private static final String TEST_URL = "/chrome/test/data/browsing_data/e.html";
    private static final String TEST_URL_DISPLAY_TITLE = "My_title";

    @Mock private static AutocompleteController sACController;

    @BeforeClass
    public static void beforeClass() {
        AutocompleteControllerProvider.setControllerForTesting(sACController);
        sTestServer = sActivityTestRule.getTestServer();
    }

    @Test
    @LargeTest
    public void testFlatBufferTabStateRegularTab() throws ExecutionException, IOException {
        TabState state = getTestTabState(false);
        File file = getTestFile(1, false);
        TabStateFileManager.saveStateInternal(file, state, false);
        TabState restoredTabState = TabStateFileManager.restoreTabStateInternal(file, false);
        verifyTabStateResult(restoredTabState, state);
    }

    @Test
    @LargeTest
    public void testFlatBufferTabStateIncognitoTab() throws ExecutionException, IOException {
        TabState state = getTestTabState(true);
        File file = getTestFile(2, true);
        TabStateFileManager.saveStateInternal(file, state, false);
        TabState restoredTabState = TabStateFileManager.restoreTabStateInternal(file, false);
        verifyTabStateResult(restoredTabState, state);
    }

    private static TabState getTestTabState(boolean isIncognito) throws ExecutionException {
        TabState state = new TabState();
        state.parentId = 4;
        state.rootId = 5;
        state.themeColor = TabState.UNSPECIFIED_THEME_COLOR;
        state.tabLaunchTypeAtCreation = TabLaunchType.FROM_CHROME_UI;
        state.userAgent = UserAgentType.DESKTOP;
        state.lastNavigationCommittedTimestampMillis = 42L;
        state.timestampMillis = 41L;
        state.isIncognito = isIncognito;
        String url = sTestServer.getURL(TEST_URL);
        Tab tab = sActivityTestRule.loadUrlInNewTab(url);
        state.contentsState =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> TabStateExtractor.getWebContentsState(tab));
        state.openerAppId = "openerAppId";
        return state;
    }

    private File getTestFile(int tabId, boolean isEncrypted) throws IOException {
        String filePrefix;
        if (isEncrypted) {
            filePrefix = TabStateFileManager.FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO;
        } else {
            filePrefix = TabStateFileManager.FLATBUFFER_SAVED_TAB_STATE_FILE_PREFIX;
        }
        return temporaryFolder.newFile(String.format(Locale.US, "%s%d", filePrefix, tabId));
    }

    private static void verifyTabStateResult(TabState actual, TabState expected) {
        Assert.assertNotNull(expected);
        Assert.assertEquals(expected.parentId, actual.parentId);
        Assert.assertEquals(expected.rootId, actual.rootId);
        Assert.assertEquals(expected.openerAppId, actual.openerAppId);
        Assert.assertEquals(expected.tabLaunchTypeAtCreation, actual.tabLaunchTypeAtCreation);
        Assert.assertEquals(
                expected.lastNavigationCommittedTimestampMillis,
                actual.lastNavigationCommittedTimestampMillis);
        Assert.assertEquals(expected.timestampMillis, actual.timestampMillis);
        Assert.assertEquals(expected.themeColor, actual.themeColor);
        ByteBufferTestUtils.verifyByteBuffer(
                expected.contentsState.buffer(), actual.contentsState.buffer());
        Assert.assertTrue(
                actual.contentsState.getDisplayTitleFromState().contains(TEST_URL_DISPLAY_TITLE));
        Assert.assertEquals(
                expected.contentsState.getVirtualUrlFromState(),
                actual.contentsState.getVirtualUrlFromState());
        Assert.assertEquals(
                expected.contentsState.getDisplayTitleFromState(),
                actual.contentsState.getDisplayTitleFromState());
    }
}
