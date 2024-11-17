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

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ByteBufferTestUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** Test relating to FlatBuffer portion of {@link TabStateFileManager} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
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
    private static CipherFactory sCipherFactory;

    private static final String TEST_URL = "/chrome/test/data/browsing_data/e.html";
    private static final String TEST_URL_DISPLAY_TITLE = "My_title";

    @BeforeClass
    public static void beforeClass() {
        sTestServer = sActivityTestRule.getTestServer();
        sCipherFactory = new CipherFactory();
    }

    @Test
    @LargeTest
    public void testFlatBufferTabStateRegularTab() throws ExecutionException, IOException {
        TabState state = getTestTabState(false);
        File file = getTestFile(1, false);
        TabStateFileManager.saveStateInternal(file, state, false, sCipherFactory);
        TabState restoredTabState =
                TabStateFileManager.restoreTabStateInternal(file, false, sCipherFactory);
        verifyTabStateResult(restoredTabState, state);
    }

    @Test
    @LargeTest
    public void testFlatBufferTabStateIncognitoTab() throws ExecutionException, IOException {
        TabState state = getTestTabState(true);
        File file = getTestFile(2, true);
        TabStateFileManager.saveStateInternal(file, state, false, sCipherFactory);
        TabState restoredTabState =
                TabStateFileManager.restoreTabStateInternal(file, false, sCipherFactory);
        verifyTabStateResult(restoredTabState, state);
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.TAB_STATE_FLAT_BUFFER)
    public void testFlatBufferCleanup() throws IOException, TimeoutException, ExecutionException {
        List<File> flatBufferFiles = new ArrayList<>();
        List<File> legacyHandWrittenFiles = new ArrayList<>();
        for (int tabId = 0; tabId < 4; tabId++) {
            legacyHandWrittenFiles.add(getLegacyTestFile(tabId, /* isEncrypted= */ tabId % 2 == 0));
            flatBufferFiles.add(getTestFile(tabId, /* isEncrypted= */ tabId % 2 == 0));
        }

        for (int tabId = 0; tabId < 4; tabId++) {
            TabState tabState =
                    getTestTabState(
                            /* isIncognito */
                            tabId % 2 == 0);
            TabStateFileManager.saveStateInternal(
                    legacyHandWrittenFiles.get(tabId),
                    tabState,
                    /* encrypted= */ tabId % 2 == 0,
                    sCipherFactory);
            TabStateFileManager.saveStateInternal(
                    flatBufferFiles.get(tabId),
                    tabState,
                    /* encrypted= */ tabId % 2 == 0,
                    sCipherFactory);
        }
        for (File file :
                Stream.concat(legacyHandWrittenFiles.stream(), flatBufferFiles.stream())
                        .collect(Collectors.toList())) {
            Assert.assertTrue("File " + file + " should exist.", file.exists());
        }
        TabStateFileManager.cleanupUnusedFiles(flatBufferFiles.get(0).getParentFile());
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    for (File file : flatBufferFiles) {
                        Criteria.checkThat(
                                "File " + file + " should no longer exist.",
                                file.exists(),
                                Matchers.is(false));
                    }
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    for (File file : legacyHandWrittenFiles) {
                        Criteria.checkThat(
                                "File " + file + " should still exist.",
                                file.exists(),
                                Matchers.is(true));
                    }
                });
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_STATE_FLAT_BUFFER)
    public void testFlatBufferMetrics() throws ExecutionException, IOException {
        TabState state = getTestTabState(false);
        File file = getTestFile(1, false);
        TabStateFileManager.saveStateInternal(file, state, false, sCipherFactory);
        var histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabState.RestoreMethod",
                        TabStateFileManager.TabStateRestoreMethod.FLATBUFFER);
        TabState restoredTabState =
                TabStateFileManager.restoreTabState(temporaryFolder.getRoot(), 1, sCipherFactory);
        Assert.assertNotNull(restoredTabState);
        histograms.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_STATE_FLAT_BUFFER)
    public void testLegacyHandWrittenMetrics() throws ExecutionException, IOException {
        TabState state = getTestTabState(false);
        File file = getLegacyTestFile(1, false);
        TabStateFileManager.saveStateInternal(file, state, false, sCipherFactory);
        var histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabState.RestoreMethod",
                        TabStateFileManager.TabStateRestoreMethod.LEGACY_HAND_WRITTEN);
        TabState restoredTabState =
                TabStateFileManager.restoreTabState(temporaryFolder.getRoot(), 1, sCipherFactory);
        Assert.assertNotNull(restoredTabState);
        histograms.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_STATE_FLAT_BUFFER)
    public void testCorruptTabStateFile() throws ExecutionException, IOException {
        File legacyFile = getLegacyTestFile(1, false);
        FileOutputStream legacyOutputStream = new FileOutputStream(legacyFile);
        legacyOutputStream.write(new byte[] {1, 2, 3, 4, 5});
        legacyOutputStream.close();
        File flatBufferFile = getTestFile(1, false);
        FileOutputStream flatBufferOutputStream = new FileOutputStream(flatBufferFile);
        flatBufferOutputStream.write(new byte[] {6, 7, 8, 9, 10});
        flatBufferOutputStream.close();
        var histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabState.RestoreMethod",
                        TabStateFileManager.TabStateRestoreMethod.FAILED);
        TabState restoredTabState =
                TabStateFileManager.restoreTabState(temporaryFolder.getRoot(), 1, sCipherFactory);
        Assert.assertNull(restoredTabState);
        histograms.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_STATE_FLAT_BUFFER)
    public void testFlatBufferFormatIncognito() throws ExecutionException {
        TabState state = getTestTabState(/* isIncognito= */ true);
        TabStateFileManager.saveStateInternal(
                TabStateFileManager.getTabStateFile(
                        temporaryFolder.getRoot(),
                        /* tabId= */ 4,
                        /* encrypted= */ true,
                        /* isFlatBuffer= */ true),
                state,
                /* isEncrypted= */ true,
                sCipherFactory);
        TabState restored =
                TabStateFileManager.restoreTabState(temporaryFolder.getRoot(), 4, sCipherFactory);
        Assert.assertTrue(restored.isIncognito);
    }

    private static TabState getTestTabState(boolean isIncognito) throws ExecutionException {
        TabState state = new TabState();
        state.parentId = 4;
        state.rootId = 5;
        state.tabGroupId = new Token(1L, 2L);
        state.themeColor = TabState.UNSPECIFIED_THEME_COLOR;
        state.tabLaunchTypeAtCreation = TabLaunchType.FROM_CHROME_UI;
        state.userAgent = UserAgentType.DESKTOP;
        state.lastNavigationCommittedTimestampMillis = 42L;
        state.timestampMillis = 41L;
        state.tabHasSensitiveContent = true;
        state.isIncognito = isIncognito;
        String url = sTestServer.getURL(TEST_URL);
        Tab tab = sActivityTestRule.loadUrlInNewTab(url);
        state.contentsState =
                ThreadUtils.runOnUiThreadBlocking(() -> TabStateExtractor.getWebContentsState(tab));
        state.openerAppId = "openerAppId";
        return state;
    }

    private File getLegacyTestFile(int tabId, boolean isEncrypted) throws IOException {
        String filePrefix;
        if (isEncrypted) {
            filePrefix = TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO;
        } else {
            filePrefix = TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX;
        }
        return temporaryFolder.newFile(String.format(Locale.US, "%s%d", filePrefix, tabId));
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
        Assert.assertEquals(expected.tabGroupId, actual.tabGroupId);
        Assert.assertEquals(expected.openerAppId, actual.openerAppId);
        Assert.assertEquals(expected.tabLaunchTypeAtCreation, actual.tabLaunchTypeAtCreation);
        Assert.assertEquals(
                expected.lastNavigationCommittedTimestampMillis,
                actual.lastNavigationCommittedTimestampMillis);
        Assert.assertEquals(expected.timestampMillis, actual.timestampMillis);
        Assert.assertEquals(expected.themeColor, actual.themeColor);
        Assert.assertEquals(expected.tabHasSensitiveContent, actual.tabHasSensitiveContent);
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
