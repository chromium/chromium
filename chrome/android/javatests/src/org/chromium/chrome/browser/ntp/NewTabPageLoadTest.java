// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertNotEquals;

import android.graphics.Bitmap;
import android.os.Handler;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.favicon.IconType;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileVisualType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for events around the loading of a New Tab Page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NewTabPageLoadTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionDeps = new SuggestionsDependenciesRule();

    private Tab mTab;
    private EmbeddedTestServer mTestServer;
    private AutoVerifyingMostVisitedSites mMostVisitedSites;

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        mMostVisitedSites = new AutoVerifyingMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mTestServer.getURLs("/site1", "/site2"));
        mSuggestionDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mSuggestionDeps.getFactory().largeIconBridge = new AsyncMockLargeIconBridge();

        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    public void testTilesTypeInitialisedWhenPageLoaded() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
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
        public boolean getLargeIconForUrl(String pageUrl, int desiredSizePx,
                final LargeIconBridge.LargeIconCallback callback) {
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    callback.onLargeIconAvailable(
                            Bitmap.createBitmap(148, 148, Bitmap.Config.ALPHA_8), 0, false,
                            IconType.INVALID);
                }
            }, 0);

            return true;
        }

        @Override
        public void destroy() {
            // Empty to avoid calling nativeDestroy inappropriately.
        }
    }
}
