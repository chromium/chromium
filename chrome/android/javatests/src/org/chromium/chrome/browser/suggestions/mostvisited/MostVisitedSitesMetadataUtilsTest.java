// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.core.util.AtomicFile;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

/** Instrumentation tests for {@link MostVisitedSitesMetadataUtils}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MostVisitedSitesMetadataUtilsTest {
    @Rule public ChromeTabbedActivityTestRule mTestSetupRule = new ChromeTabbedActivityTestRule();

    private MostVisitedSitesMetadataUtils mMostVisitedSitesMetadataUtils;

    @Before
    public void setUp() {
        mTestSetupRule.startMainActivityOnBlankPage();
        mMostVisitedSitesMetadataUtils = MostVisitedSitesMetadataUtils.getInstance();
    }

    @Test
    @SmallTest
    public void testSaveRestoreConsistency() throws InterruptedException, IOException {
        List<Tile> expectedSuggestionTiles = createFakeSiteSuggestionTiles1();

        // Get old file and ensure to delete it.
        File oldFile = MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory();
        assertTrue(oldFile.delete() && !oldFile.exists());

        // Save suggestion lists to file.
        final CountDownLatch latch = new CountDownLatch(1);
        MostVisitedSitesMetadataUtils.saveSuggestionListsToFile(
                expectedSuggestionTiles, latch::countDown);

        // Wait util the file has been saved.
        latch.await();

        // Restore list from file after saving finished.
        List<Tile> sitesAfterRestore = MostVisitedSitesMetadataUtils.restoreFileToSuggestionLists();

        // Ensure that the new list equals to old list.
        assertEquals(expectedSuggestionTiles.size(), sitesAfterRestore.size());
        for (int i = 0; i < expectedSuggestionTiles.size(); i++) {
            assertEquals(
                    expectedSuggestionTiles.get(i).getData(), sitesAfterRestore.get(i).getData());
        }
    }

    @Test(expected = IOException.class)
    @SmallTest
    public void testRestoreException() throws IOException {
        // Get old file and ensure to delete it.
        File oldFile = MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory();
        assertTrue(oldFile.delete() || !oldFile.exists());

        // Call restore function and ensure it throws an IOException.
        MostVisitedSitesMetadataUtils.restoreFileToSuggestionLists();
    }

    /**
     * Test when current task is not finished, all coming tasks will be set as the pending task.
     * Besides, the latest task will override the old one.
     */
    @Test
    @SmallTest
    public void testCurrentNotNull() {
        mMostVisitedSitesMetadataUtils.setCurrentTaskForTesting(CallbackUtils.emptyRunnable());

        Runnable task1 =
                () ->
                        mMostVisitedSitesMetadataUtils.saveSuggestionListsToFile(
                                createFakeSiteSuggestionTiles1());

        List<Tile> task2Tiles = createFakeSiteSuggestionTiles2();
        Runnable task2 = () -> mMostVisitedSitesMetadataUtils.saveSuggestionListsToFile(task2Tiles);

        // If current task is not null, all saving tasks should be set as pending task.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task1.run();
                    task2.run();
                });

        // newTopSites1 should be skipped and newTopSites2 should be the pending task.
        assertEquals(
                task2Tiles.size(),
                mMostVisitedSitesMetadataUtils.getPendingTaskTilesNumForTesting());
    }

    /**
     * Test when current task is finished, the pending task should be set as current task and run.
     */
    @Test
    @SmallTest
    public void testTasksContinuity() {
        AtomicBoolean isPendingRun = new AtomicBoolean(false);

        // Set and run current task.
        assertNull(mMostVisitedSitesMetadataUtils.getCurrentTaskForTesting());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMostVisitedSitesMetadataUtils.saveSuggestionListsToFile(
                                createFakeSiteSuggestionTiles1()));

        // When current task is not finished, set pending task.
        assertNotNull(mMostVisitedSitesMetadataUtils.getCurrentTaskForTesting());
        mMostVisitedSitesMetadataUtils.setPendingTaskForTesting(() -> isPendingRun.set(true));

        // isPendingRun should eventually become true.
        CriteriaHelper.pollInstrumentationThread(isPendingRun::get);
    }

    /**
     * Test that if the allowlist icon path (a deprecated field) is set on an old site suggestion
     * that the site suggestion is being deserialized correctly.
     */
    @Test
    @SmallTest
    public void testTileWithAllowlistIconPath() throws IOException {
        // Create a site suggestion that has the allowlist icon path set.
        SiteSuggestion expectedSiteSuggestion = createFakeSiteSuggestionTiles2().get(0).getData();
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream stream = new DataOutputStream(output);
        stream.writeInt(/* topSitesCount= */ 1);
        stream.writeInt(/* cacheVersion= */ 1);
        stream.writeInt(/* index= */ 0);
        stream.writeUTF(expectedSiteSuggestion.title);
        stream.writeUTF(expectedSiteSuggestion.url.serialize());
        // Hardcode this since the field is deprecated and can't be set in another way.
        stream.writeUTF("allowlistIcon");
        stream.writeInt(expectedSiteSuggestion.titleSource);
        stream.writeInt(expectedSiteSuggestion.source);
        stream.writeInt(expectedSiteSuggestion.sectionType);
        stream.close();
        byte[] listData = output.toByteArray();

        // Save the site suggestion.
        File topSitesDirectory = MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory();
        File topSitesFile = new File(topSitesDirectory, "top_sites");
        AtomicFile file = new AtomicFile(topSitesFile);
        FileOutputStream fileStream = null;
        fileStream = file.startWrite();
        fileStream.write(listData, 0, listData.length);
        file.finishWrite(fileStream);

        // Restore list from file after saving finished.
        List<Tile> sitesAfterRestore = MostVisitedSitesMetadataUtils.restoreFileToSuggestionLists();
        // Ensure that the new suggestion equals to old suggestion.
        assertEquals(1, sitesAfterRestore.size());
        assertEquals(sitesAfterRestore.get(0).getData(), expectedSiteSuggestion);
    }

    private static List<Tile> createFakeSiteSuggestionTiles1() {
        List<Tile> suggestionTiles = new ArrayList<>();
        SiteSuggestion data =
                new SiteSuggestion(
                        "0 TOP_SITES",
                        new GURL("https://www.foo.com"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED);
        suggestionTiles.add(new Tile(data, 0));

        data =
                new SiteSuggestion(
                        "1 ALLOWLIST",
                        new GURL("https://www.bar.com"),
                        TileTitleSource.UNKNOWN,
                        TileSource.ALLOWLIST,
                        TileSectionType.PERSONALIZED);
        suggestionTiles.add(new Tile(data, 1));

        return suggestionTiles;
    }

    private static List<Tile> createFakeSiteSuggestionTiles2() {
        List<Tile> suggestionTiles = new ArrayList<>();
        SiteSuggestion data =
                new SiteSuggestion(
                        "0 TOP_SITES",
                        new GURL("https://www.baz.com"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED);
        suggestionTiles.add(new Tile(data, 0));

        return suggestionTiles;
    }
}
