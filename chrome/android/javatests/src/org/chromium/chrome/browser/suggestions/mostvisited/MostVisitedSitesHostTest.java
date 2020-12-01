// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;

import androidx.core.util.AtomicFile;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Instrumentation tests for {@link MostVisitedSitesHost}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MostVisitedSitesHostTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestSetupRule = new ChromeTabbedActivityTestRule();

    MostVisitedSitesHost mMostVisitedSitesHost;

    @Before
    public void setUp() {
        mTestSetupRule.startMainActivityOnBlankPage();
        MostVisitedSitesHost.setSkipRestoreFromDiskForTesting();
        mMostVisitedSitesHost = MostVisitedSitesHost.getInstance();
    }

    @Test
    @SmallTest
    public void testNextAvailableId() {
        Set<GURL> newTopSiteUrls = new HashSet<>();
        GURL url0 = new GURL("https://www.0.com");
        GURL url1 = new GURL("https://www.1.com");
        GURL url2 = new GURL("https://www.2.com");
        GURL url3 = new GURL("https://www.3.com");

        // Update map and set.
        newTopSiteUrls.add(url0);
        newTopSiteUrls.add(url1);
        mMostVisitedSitesHost.getUrlsToUpdateFaviconForTesting().addAll(newTopSiteUrls);

        // The next available ID should be 0.
        assertEquals(0, mMostVisitedSitesHost.getNextAvailableId(0, newTopSiteUrls));
        mMostVisitedSitesHost.getUrlToIDMapForTesting().put(url0, 0);
        mMostVisitedSitesHost.buildIdToUrlMap();

        // The next available ID should be 1.
        assertEquals(1, mMostVisitedSitesHost.getNextAvailableId(1, newTopSiteUrls));
        mMostVisitedSitesHost.getUrlToIDMapForTesting().put(url1, 1);
        mMostVisitedSitesHost.buildIdToUrlMap();

        // Create a new batch of SiteSuggestions.
        newTopSiteUrls.clear();
        newTopSiteUrls.add(url0);
        newTopSiteUrls.add(url2);
        newTopSiteUrls.add(url3);

        // The next available ID should be ID for "https://www.1.com".
        assertEquals(1, mMostVisitedSitesHost.getNextAvailableId(0, newTopSiteUrls));
        mMostVisitedSitesHost.getUrlToIDMapForTesting().put(url2, 1);

        // After setting 1 to "https://www.2.com", the next available ID should be 2.
        assertEquals(2, mMostVisitedSitesHost.getNextAvailableId(2, newTopSiteUrls));
    }

    @Test
    @SmallTest
    public void testUpdateMapAndSet() {
        GURL url0 = new GURL("https://www.0.com");
        GURL url2 = new GURL("https://www.2.com");
        mMostVisitedSitesHost.getUrlToIDMapForTesting().clear();
        mMostVisitedSitesHost.getUrlsToUpdateFaviconForTesting().clear();
        mMostVisitedSitesHost.getIdToUrlMapForTesting().clear();

        Set<GURL> newUrls;
        Set<Integer> expectedIdsInMap = new HashSet<>();
        AtomicBoolean isUpdated = new AtomicBoolean(false);

        // Based on the first batch of sites, update the map and set.
        List<SiteSuggestion> newTopSites = createFakeSiteSuggestions1();
        newUrls = getUrls(newTopSites);
        mMostVisitedSitesHost.updateMapAndSetForNewSites(newTopSites, () -> isUpdated.set(true));
        CriteriaHelper.pollInstrumentationThread(isUpdated::get);

        // Check the map and set.
        expectedIdsInMap.add(0);
        expectedIdsInMap.add(1);
        expectedIdsInMap.add(2);
        checkMapAndSet(newUrls, newUrls, expectedIdsInMap, newTopSites);

        // Record ID of "https://www.0.com" for checking later.
        int expectedId0 =
                mMostVisitedSitesHost.getUrlToIDMapForTesting().get(new GURL("https://www.0.com"));

        // Emulate saving favicons for the first batch of sites except "https://www.2.com".
        int expectedId2 =
                mMostVisitedSitesHost.getUrlToIDMapForTesting().get(new GURL("https://www.2.com"));
        for (int i = 0; i < 3; i++) {
            if (i == expectedId2) {
                continue;
            }
            createBitmapAndWriteToFile(String.valueOf(i));
        }

        // Based on the second batch of sites, update the map and set.
        newTopSites = createFakeSiteSuggestions2();
        newUrls = getUrls(newTopSites);
        isUpdated.set(false);
        mMostVisitedSitesHost.updateMapAndSetForNewSites(newTopSites, () -> isUpdated.set(true));
        CriteriaHelper.pollInstrumentationThread(isUpdated::get);

        // Check the map and set.
        Set<GURL> expectedUrlsToFetchIcon = new HashSet<>(newUrls);
        expectedUrlsToFetchIcon.remove(url0);
        expectedIdsInMap.add(3);
        checkMapAndSet(expectedUrlsToFetchIcon, newUrls, expectedIdsInMap, newTopSites);
        assertEquals(expectedId0, (int) mMostVisitedSitesHost.getUrlToIDMapForTesting().get(url0));
        assertEquals(expectedId2, (int) mMostVisitedSitesHost.getUrlToIDMapForTesting().get(url2));
    }

    /**
     * Test when synchronization of metadata stored on disk hasn't finished yet, all coming tasks
     * will be set as the pending task. Besides, the latest task will override the old one.
     */
    @Test
    @SmallTest
    public void testSyncNotFinished() {
        List<SiteSuggestion> newTopSites1 = createFakeSiteSuggestions1();
        List<SiteSuggestion> newTopSites2 = createFakeSiteSuggestions2();

        mMostVisitedSitesHost.setIsSyncedForTesting(false);

        // If restoring from disk is not finished, all coming tasks should be set as the pending
        // task.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMostVisitedSitesHost.saveMostVisitedSitesInfo(newTopSites1);
            mMostVisitedSitesHost.saveMostVisitedSitesInfo(newTopSites2);
        });

        // newTopSites1 should be skipped and newTopSites2 should be the pending task.
        assertEquals(newTopSites2.size(),
                mMostVisitedSitesHost.getPendingFilesNeedToSaveCountForTesting() - 1);
    }

    /**
     * Test when current task is not finished, all coming tasks will be set as the pending task.
     * Besides, the latest task will override the old one.
     */
    @Test
    @SmallTest
    public void testCurrentNotNull() {
        List<SiteSuggestion> newTopSites1 = createFakeSiteSuggestions1();
        List<SiteSuggestion> newTopSites2 = createFakeSiteSuggestions2();

        mMostVisitedSitesHost.setIsSyncedForTesting(true);
        mMostVisitedSitesHost.setCurrentTaskForTesting(() -> {});

        // If current task is not null, all saving tasks should be set as pending task.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMostVisitedSitesHost.saveMostVisitedSitesInfo(newTopSites1);
            mMostVisitedSitesHost.saveMostVisitedSitesInfo(newTopSites2);
        });

        // newTopSites1 should be skipped and newTopSites2 should be the pending task.
        assertEquals(newTopSites2.size(),
                mMostVisitedSitesHost.getPendingFilesNeedToSaveCountForTesting() - 1);
    }

    /**
     * Test when current task is finished, the pending task should be set as current task and run.
     */
    @Test
    @SmallTest
    public void testTasksContinuity() {
        AtomicBoolean isPendingRun = new AtomicBoolean(false);

        // Set and run current task.
        mMostVisitedSitesHost.setIsSyncedForTesting(true);
        mMostVisitedSitesHost.setCurrentTaskForTesting(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMostVisitedSitesHost.saveMostVisitedSitesInfo(createFakeSiteSuggestions1());
        });

        // When current task is not finished, set pending task.
        assertTrue(mMostVisitedSitesHost.getCurrentFilesNeedToSaveCountForTesting() > 0);
        mMostVisitedSitesHost.setPendingTaskForTesting(() -> isPendingRun.set(true));

        // isPendingRun should eventually become true.
        CriteriaHelper.pollInstrumentationThread(isPendingRun::get);
    }

    private void checkMapAndSet(Set<GURL> expectedUrlsToFetchIcon, Set<GURL> expectedUrlsInMap,
            Set<Integer> expectedIdsInMap, List<SiteSuggestion> newSiteSuggestions) {
        // Check whether mExpectedSiteSuggestions' faviconIDs have been updated.
        for (SiteSuggestion siteData : newSiteSuggestions) {
            assertEquals((int) mMostVisitedSitesHost.getUrlToIDMapForTesting().get(siteData.url),
                    siteData.faviconId);
        }

        Set<GURL> urlsToUpdateFavicon = mMostVisitedSitesHost.getUrlsToUpdateFaviconForTesting();
        Map<GURL, Integer> urlToIDMap = mMostVisitedSitesHost.getUrlToIDMapForTesting();

        assertEquals(expectedUrlsToFetchIcon, urlsToUpdateFavicon);
        assertThat(expectedUrlsInMap, containsInAnyOrder(urlToIDMap.keySet().toArray()));
        assertThat(expectedIdsInMap, containsInAnyOrder(urlToIDMap.values().toArray()));
    }

    private static List<SiteSuggestion> createFakeSiteSuggestions1() {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        siteSuggestions.add(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.0.com"), "",
                TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("1 ALLOWLIST", new GURL("https://www.1.com"), "",
                TileTitleSource.UNKNOWN, TileSource.ALLOWLIST, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("2 TOP_SITES", new GURL("https://www.2.com"), "",
                TileTitleSource.UNKNOWN, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        return siteSuggestions;
    }

    private static List<SiteSuggestion> createFakeSiteSuggestions2() {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        siteSuggestions.add(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.0.com"), "",
                TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("2 TOP_SITES", new GURL("https://www.2.com"), "",
                TileTitleSource.UNKNOWN, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("3 TOP_SITES", new GURL("https://www.3.com"), "",
                TileTitleSource.UNKNOWN, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("4 TOP_SITES", new GURL("https://www.4.com"), "",
                TileTitleSource.UNKNOWN, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        return siteSuggestions;
    }

    private Set<GURL> getUrls(List<SiteSuggestion> newSuggestions) {
        Set<GURL> newTopSiteUrls = new HashSet<>();
        for (SiteSuggestion topSiteData : newSuggestions) {
            newTopSiteUrls.add(topSiteData.url);
        }
        return newTopSiteUrls;
    }

    private static void createBitmapAndWriteToFile(String fileName) {
        final Bitmap testBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        // Save bitmap to file.
        File metadataFile =
                new File(MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory(), fileName);
        AtomicFile file = new AtomicFile(metadataFile);
        FileOutputStream stream;
        try {
            stream = file.startWrite();
            testBitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
            file.finishWrite(stream);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
