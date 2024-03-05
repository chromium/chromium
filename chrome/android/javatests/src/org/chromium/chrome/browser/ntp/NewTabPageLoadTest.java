// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.os.Handler;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileVisualType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

/** Tests for events around the loading of a New Tab Page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NewTabPageLoadTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public SuggestionsDependenciesRule mSuggestionDeps = new SuggestionsDependenciesRule();

    private Tab mTab;
    private EmbeddedTestServer mTestServer;
    private AutoVerifyingMostVisitedSites mMostVisitedSites;

    @Before
    public void setUp() throws Exception {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mMostVisitedSites = new AutoVerifyingMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mTestServer.getURLs("/site1", "/site2"));
        mSuggestionDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mSuggestionDeps.getFactory().largeIconBridge = new AsyncMockLargeIconBridge();

        mTab = sActivityTestRule.getActivity().getActivityTab();
    }

    @Test
    @SmallTest
    public void testTilesTypeInitialisedWhenPageLoaded() {
        sActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
        assertTrue(mMostVisitedSites.pageImpressionRecorded);
    }

    private static class AutoVerifyingMostVisitedSites extends FakeMostVisitedSites {
        public boolean pageImpressionRecorded;

        @Override
        public void recordPageImpression(int tilesCount) {
            pageImpressionRecorded = true;
        }

        @Override
        public void recordTileImpression(Tile tile) {
            assertNotEquals(TileVisualType.NONE, tile.getType());
        }
    }

    private static class AsyncMockLargeIconBridge extends LargeIconBridge {
        @Override
        public boolean getLargeIconForUrl(
                GURL pageUrl, int desiredSizePx, final LargeIconBridge.LargeIconCallback callback) {
            new Handler()
                    .postDelayed(
                            new Runnable() {
                                @Override
                                public void run() {
                                    callback.onLargeIconAvailable(
                                            Bitmap.createBitmap(148, 148, Bitmap.Config.ALPHA_8),
                                            0,
                                            false,
                                            IconType.INVALID);
                                }
                            },
                            0);

            return true;
        }

        @Override
        public void destroy() {
            // Empty to avoid calling nativeDestroy inappropriately.
        }
    }
}
