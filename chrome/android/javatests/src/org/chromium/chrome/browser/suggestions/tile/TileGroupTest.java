// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.MvtRemovedSnackbarFacility;
import org.chromium.chrome.test.transit.ntp.MvtUnpinnedSnackbarFacility;
import org.chromium.chrome.test.transit.ntp.MvtsFacility;
import org.chromium.chrome.test.transit.ntp.MvtsTileFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Set;

/** Instrumentation tests for {@link TileGroup} on the New Tab Page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TileGroupTest {
    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    private static final String[] FAKE_MOST_VISITED_URLS =
            new String[] {
                "/chrome/test/data/android/navigate/one.html",
                "/chrome/test/data/android/navigate/two.html",
                "/chrome/test/data/android/navigate/three.html"
            };
    private static final GURL FAKE_CUSTOM_LINK_URL = new GURL("https://www.google.com/");

    private String[] mSiteSuggestionUrls;
    private FakeMostVisitedSites mMostVisitedSites;
    private List<SiteSuggestion> mSiteSuggestions;
    private EmbeddedTestServer mTestServer;
    private WebPageStation mInitialPage;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mSiteSuggestionUrls = mTestServer.getURLs(FAKE_MOST_VISITED_URLS);

        mMostVisitedSites = new FakeMostVisitedSites();
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;
        mSiteSuggestions = FakeMostVisitedSites.createSiteSuggestions(mSiteSuggestionUrls);
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);

        mInitialPage = mActivityTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.PHONE})
    public void testDismissTileWithContextMenu_Phones() {
        doTestDismissTileWithContextMenuImpl();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    public void testDismissTileWithContextMenu_Tablets() {
        doTestDismissTileWithContextMenuImpl();
    }

    private MvtRemovedSnackbarFacility doTestDismissTileWithContextMenuImpl() {
        RegularNewTabPageStation ntp =
                mInitialPage.loadPageProgrammatically(
                        UrlConstants.NTP_URL, RegularNewTabPageStation.newBuilder());
        MvtsFacility mvts = ntp.focusOnMvts(mSiteSuggestions);
        MvtsTileFacility tile = mvts.ensureTileIsDisplayedAndGet(0);

        // Dismiss the tile using the context menu.
        List<SiteSuggestion> siteSuggestionsAfterRemove =
                mSiteSuggestions.subList(1, mSiteSuggestions.size());
        var snackbar =
                tile.openContextMenu().selectRemove(siteSuggestionsAfterRemove, mMostVisitedSites);
        assertTrue(mMostVisitedSites.isUrlBlocklisted(new GURL(mSiteSuggestionUrls[0])));

        return snackbar;
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.PHONE})
    public void testDismissTileUndo_Phones() {
        doTestDismissTileUndoImpl();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    public void testDismissTileUndo_Tablets() {
        doTestDismissTileUndoImpl();
    }

    private void doTestDismissTileUndoImpl() {
        MvtRemovedSnackbarFacility snackbar = doTestDismissTileWithContextMenuImpl();

        // Undo removal with the snackbar.
        snackbar.undo(mMostVisitedSites);
        assertFalse(mMostVisitedSites.isUrlBlocklisted(new GURL(mSiteSuggestionUrls[0])));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest(message = "Disabled due to crbug.com/454314629")
    public void testUnpinCustomTile_Phones() {
        doTestUnpinCustomTileImpl();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @DisabledTest(message = "Disabled due to crbug.com/454314629")
    public void testUnpinCustomTile_Tablets() {
        doTestUnpinCustomTileImpl();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest(message = "Disabled due to crbug.com/454314629")
    public void testUnpinCustomTile_undo_Phones() {
        var snackbar = doTestUnpinCustomTileImpl();

        // Undo the unpin.
        snackbar.undo(mMostVisitedSites);
        assertTrue(mMostVisitedSites.hasCustomLink(FAKE_CUSTOM_LINK_URL));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
    @DisabledTest(message = "Disabled due to crbug.com/454314629")
    public void testUnpinCustomTile_undo_Tablets() {
        var snackbar = doTestUnpinCustomTileImpl();

        // Undo the unpin.
        snackbar.undo(mMostVisitedSites);
        assertTrue(mMostVisitedSites.hasCustomLink(FAKE_CUSTOM_LINK_URL));
    }

    private MvtUnpinnedSnackbarFacility doTestUnpinCustomTileImpl() {
        mMostVisitedSites.addCustomLink("Custom Link", FAKE_CUSTOM_LINK_URL, 0);

        RegularNewTabPageStation ntp =
                mInitialPage.loadPageProgrammatically(
                        UrlConstants.NTP_URL, RegularNewTabPageStation.newBuilder());
        MvtsFacility mvts =
                ntp.focusOnMvts(
                        mMostVisitedSites.getCombinedSuggestions(),
                        /* separatorIndices= */ Set.of(1));
        MvtsTileFacility tile = mvts.ensureTileIsDisplayedAndGet(0);

        // Unpin the tile using the context menu.
        var snackbar = tile.openContextMenu().selectUnpin(mSiteSuggestions, mMostVisitedSites);
        assertFalse(mMostVisitedSites.hasCustomLink(FAKE_CUSTOM_LINK_URL));
        return snackbar;
    }
}
