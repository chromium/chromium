// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.url.GURL;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * Instrumentation tests for {@link MostVisitedSitesMetadataUtils}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MostVisitedSitesMetadataUtilsTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestSetupRule = new ChromeTabbedActivityTestRule();

    private List<SiteSuggestion> mExpectedSiteSuggestions;

    @Before
    public void setUp() {
        mTestSetupRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    public void testSaveRestoreConsistency() throws InterruptedException, IOException {
        mExpectedSiteSuggestions = createFakeSiteSuggestions();

        // Get old file and ensure to delete it.
        File oldFile = MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory();
        assertTrue(oldFile.delete() && !oldFile.exists());

        // Save suggestion lists to file.
        final CountDownLatch latch = new CountDownLatch(1);
        MostVisitedSitesMetadataUtils.saveSuggestionListsToFile(
                mExpectedSiteSuggestions, latch::countDown);

        // Wait util the file has been saved.
        latch.await();

        // Restore list from file after saving finished.
        List<SiteSuggestion> sitesAfterRestore =
                MostVisitedSitesMetadataUtils.restoreFileToSuggestionLists();

        // Ensure that the new list equals to old list.
        assertEquals(mExpectedSiteSuggestions, sitesAfterRestore);
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

    private static List<SiteSuggestion> createFakeSiteSuggestions() {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        siteSuggestions.add(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.foo.com"), "",
                TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("1 ALLOWLIST", new GURL("https://www.bar.com"), "",
                TileTitleSource.UNKNOWN, TileSource.ALLOWLIST, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.get(1).faviconId = 1;
        return siteSuggestions;
    }
}
