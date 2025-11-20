// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.Token;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager.TabStateMigrationStatus;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.ByteBufferTestUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.ExecutionException;

/** Test relating to FlatBuffer portion of {@link TabStateFileManager} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabStateFlatBufferTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public TemporaryFolder temporaryFolder = new TemporaryFolder();
    private static CipherFactory sCipherFactory;

    @BeforeClass
    public static void beforeClass() {
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
    public void testFlatBufferFormatIncognito() throws ExecutionException {
        TabState state = getTestTabState(/* isIncognito= */ true);
        TabStateFileManager.saveStateInternal(
                TabStateFileManager.getTabStateFile(
                        temporaryFolder.getRoot(),
                        /* tabId= */ 4,
                        /* encrypted= */ true,
                        /* isFlatbuffer= */ true),
                state,
                /* encrypted= */ true,
                sCipherFactory);
        TabState restored =
                TabStateFileManager.restoreTabState(temporaryFolder.getRoot(), 4, sCipherFactory);
        Assert.assertTrue(restored.isIncognito);
    }

    @Test
    @LargeTest
    public void testLegacyTabStateFileDeletion() throws ExecutionException {
        TabState state = getTestTabState(/* isIncognito= */ true);
        File legacyTabStateFile =
                TabStateFileManager.getTabStateFile(
                        temporaryFolder.getRoot(),
                        /* tabId= */ 4,
                        /* encrypted= */ false,
                        /* isFlatbuffer= */ false);
        File flatBufferTabStateFile =
                TabStateFileManager.getTabStateFile(
                        temporaryFolder.getRoot(),
                        /* tabId= */ 4,
                        /* encrypted= */ false,
                        /* isFlatbuffer= */ true);
        Assert.assertFalse(legacyTabStateFile.exists());
        TabStateFileManager.saveStateInternal(
                legacyTabStateFile, state, /* encrypted= */ true, sCipherFactory);
        Assert.assertTrue(legacyTabStateFile.exists());
        TabStateFileManager.saveState(
                temporaryFolder.getRoot(),
                state,
                /* tabId= */ 4,
                /* isEncrypted= */ false,
                sCipherFactory);
        Assert.assertTrue(flatBufferTabStateFile.exists());
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "File " + legacyTabStateFile + " should not exist.",
                            legacyTabStateFile.exists(),
                            Matchers.is(false));
                });
    }

    @Test
    @LargeTest
    public void testLegacyTabStateFileMarkedForDeletion() throws ExecutionException {
        TabState state = getTestTabState(/* isIncognito= */ false);
        File legacyTabStateFile =
                TabStateFileManager.getTabStateFile(
                        temporaryFolder.getRoot(),
                        /* tabId= */ 4,
                        /* encrypted= */ false,
                        /* isFlatbuffer= */ false);
        File flatBufferTabStateFile =
                TabStateFileManager.getTabStateFile(
                        temporaryFolder.getRoot(),
                        /* tabId= */ 4,
                        /* encrypted= */ false,
                        /* isFlatbuffer= */ true);
        Assert.assertFalse(legacyTabStateFile.exists());
        Assert.assertFalse(flatBufferTabStateFile.exists());
        TabStateFileManager.saveStateInternal(
                legacyTabStateFile, state, /* encrypted= */ false, sCipherFactory);
        TabStateFileManager.saveStateInternal(
                flatBufferTabStateFile, state, /* encrypted= */ false, sCipherFactory);
        Assert.assertTrue(legacyTabStateFile.exists());
        Assert.assertTrue(flatBufferTabStateFile.exists());
        TabState restored =
                TabStateFileManager.restoreTabState(
                        temporaryFolder.getRoot(), /* id= */ 4, sCipherFactory);
        Assert.assertEquals(restored.legacyFileToDelete, legacyTabStateFile);
    }

    @Test
    @LargeTest
    public void testUnmigratedLegacyTabStateFileNotMarkedForDeletion() throws ExecutionException {
        TabState state = getTestTabState(/* isIncognito= */ false);
        File legacyTabStateFile =
                TabStateFileManager.getTabStateFile(
                        temporaryFolder.getRoot(),
                        /* tabId= */ 4,
                        /* encrypted= */ false,
                        /* isFlatbuffer= */ false);
        Assert.assertFalse(legacyTabStateFile.exists());
        TabStateFileManager.saveStateInternal(
                legacyTabStateFile, state, /* encrypted= */ false, sCipherFactory);
        TabState restored =
                TabStateFileManager.restoreTabState(
                        temporaryFolder.getRoot(), /* id= */ 4, sCipherFactory);
        Assert.assertNull(restored.legacyFileToDelete);
    }

    @Test
    @LargeTest
    public void testMigrationStatusRecordingBothFiles() throws ExecutionException, IOException {
        TabState state = getTestTabState(/* isIncognito= */ false);
        File legacyFile = getLegacyTestFile(/* tabId= */ 1, /* isEncrypted= */ false);
        File flatBufferFile = getTestFile(/* tabId= */ 1, /* isEncrypted= */ false);
        TabStateFileManager.saveStateInternal(
                legacyFile, state, /* encrypted= */ false, sCipherFactory);
        TabStateFileManager.saveStateInternal(
                flatBufferFile, state, /* encrypted= */ false, sCipherFactory);
        var histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabState.MigrationStatus",
                        TabStateMigrationStatus.FLATBUFFER_AND_LEGACY_HAND_WRITTEN);
        TabStateFileManager.recordTabStateMigrationStatus(temporaryFolder.getRoot(), /* id= */ 1);
        histograms.assertExpected();
    }

    @Test
    @LargeTest
    public void testMigrationStatusRecordingFlatBufferOnly()
            throws ExecutionException, IOException {
        TabState state = getTestTabState(/* isIncognito= */ false);
        File flatBufferFile = getTestFile(/* tabId= */ 1, /* isEncrypted= */ false);
        TabStateFileManager.saveStateInternal(
                flatBufferFile, state, /* encrypted= */ false, sCipherFactory);
        var histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabState.MigrationStatus", TabStateMigrationStatus.FLATBUFFER);
        TabStateFileManager.recordTabStateMigrationStatus(temporaryFolder.getRoot(), /* id= */ 1);
        histograms.assertExpected();
    }

    @Test
    @LargeTest
    public void testMigrationStatusRecordingLegacyOnly() throws ExecutionException, IOException {
        TabState state = getTestTabState(/* isIncognito= */ false);
        File legacyFile = getLegacyTestFile(/* tabId= */ 1, /* isEncrypted= */ false);
        TabStateFileManager.saveStateInternal(
                legacyFile, state, /* encrypted= */ false, sCipherFactory);
        var histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabState.MigrationStatus",
                        TabStateMigrationStatus.LEGACY_HAND_WRITTEN);
        TabStateFileManager.recordTabStateMigrationStatus(temporaryFolder.getRoot(), /* id= */ 1);
        histograms.assertExpected();
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
        state.isPinned = true;
        state.isIncognito = isIncognito;
        int capacity = 100;
        byte[] bytes = new byte[capacity];
        new Random().nextBytes(bytes);
        ByteBuffer buffer = ByteBuffer.allocateDirect(capacity);
        buffer.put(bytes);
        buffer.rewind();
        state.contentsState =
                new WebContentsState(buffer, WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
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
        Assert.assertEquals(expected.isPinned, actual.isPinned);
        ByteBufferTestUtils.verifyByteBuffer(
                expected.contentsState.buffer(), actual.contentsState.buffer());
        // Don't assert on the fields of the WebContentsState as it is random data in this test.
    }
}
