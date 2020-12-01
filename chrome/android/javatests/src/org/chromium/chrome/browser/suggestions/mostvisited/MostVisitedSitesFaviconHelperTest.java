// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import android.graphics.Bitmap;

import androidx.core.util.AtomicFile;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * Instrumentation tests for {@link MostVisitedSitesFaviconHelper}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MostVisitedSitesFaviconHelperTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestSetupRule = new ChromeTabbedActivityTestRule();

    private List<SiteSuggestion> mExpectedSiteSuggestions;
    private MostVisitedSitesFaviconHelper mMostVisitedSitesFaviconHelper;

    @Before
    public void setUp() {
        mTestSetupRule.startMainActivityOnBlankPage();
        mExpectedSiteSuggestions = createFakeSiteSuggestions();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LargeIconBridge largeIconBridge =
                    new LargeIconBridge(Profile.getLastUsedRegularProfile());
            mMostVisitedSitesFaviconHelper = new MostVisitedSitesFaviconHelper(
                    mTestSetupRule.getActivity(), largeIconBridge);
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1149856")
    public void testSaveFaviconsToFile() {
        // Add sites' URLs into the urlsToUpdate, except the last one.
        Set<GURL> urlsToUpdate = new HashSet<>();
        for (int i = 0; i < mExpectedSiteSuggestions.size() - 1; i++) {
            urlsToUpdate.add(mExpectedSiteSuggestions.get(i).url);
        }

        // Save file count before saving favicons since there might be other files in the directory.
        int originalFilesNum = getStateDirectorySize();

        // Call saveFaviconsToFile.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mMostVisitedSitesFaviconHelper.saveFaviconsToFile(
                                mExpectedSiteSuggestions, urlsToUpdate, null));

        // Wait util the file number equals to expected one.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    getStateDirectorySize() - originalFilesNum, Matchers.is(urlsToUpdate.size()));
        });

        // The Favicon File lists in the disk.
        File topSitesDirectory = MostVisitedSitesMetadataUtils.getStateDirectory();
        Assert.assertNotNull(topSitesDirectory);
        String[] faviconFiles = topSitesDirectory.list();
        Assert.assertNotNull(faviconFiles);
        Set<String> existingIconFiles = new HashSet<>(Arrays.asList(faviconFiles));

        // Check whether each URL's favicon exist in the disk.
        Assert.assertTrue(existingIconFiles.contains("0"));
        Assert.assertTrue(existingIconFiles.contains("1"));
        Assert.assertTrue(existingIconFiles.contains("2"));
        Assert.assertFalse(existingIconFiles.contains("3"));
    }

    private static List<SiteSuggestion> createFakeSiteSuggestions() {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();

        siteSuggestions.add(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.foo.com"), "",
                TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.add(new SiteSuggestion("1 ALLOWLIST", new GURL("https://www.bar.com"),
                "/not_exist.png", TileTitleSource.UNKNOWN, TileSource.ALLOWLIST,
                TileSectionType.PERSONALIZED, new Date()));
        siteSuggestions.add(new SiteSuggestion("2 TOP_SITES", new GURL("https://www.baz.com"),
                createBitmapAndWriteToFile(), TileTitleSource.UNKNOWN, TileSource.ALLOWLIST,
                TileSectionType.PERSONALIZED, new Date()));
        siteSuggestions.add(new SiteSuggestion("3 TOP_SITES", new GURL("https://www.qux.com"), "",
                TileTitleSource.UNKNOWN, TileSource.ALLOWLIST, TileSectionType.PERSONALIZED,
                new Date()));
        siteSuggestions.get(0).faviconId = 0;
        siteSuggestions.get(1).faviconId = 1;
        siteSuggestions.get(2).faviconId = 2;
        siteSuggestions.get(3).faviconId = 3;
        return siteSuggestions;
    }

    private static String createBitmapAndWriteToFile() {
        final Bitmap testBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        String fileName = "test.png";
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
        return metadataFile.getAbsolutePath();
    }

    private static int getStateDirectorySize() {
        return Objects.requireNonNull(MostVisitedSitesMetadataUtils.getStateDirectory().list())
                .length;
    }
}
