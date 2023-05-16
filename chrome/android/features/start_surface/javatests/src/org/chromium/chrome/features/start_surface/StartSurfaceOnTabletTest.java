// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_ON_TABLET_TEST_PARAMS;

import android.content.pm.ActivityInfo;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCarouselLayout;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesGridLayout;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesLayout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests of showing a NTP with Start surface UI at startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_TABLET, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET,
        ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@DoNotBatch(reason = "This test suite tests startup behaviors.")
public class StartSurfaceOnTabletTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    private static final String TAB_URL = "https://foo.com/";

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    @DisableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET})
    public void testStartSurfaceOnTabletDisabled() throws IOException {
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        verifyTabCountAndActiveTabUrl(
                mActivityTestRule.getActivity(), 1, TAB_URL, null /* expectHomeSurfaceUiShown */);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    public void testStartSurfaceOnTablet() throws IOException {
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that a NTP is created and set as the current Tab.
        verifyTabCountAndActiveTabUrl(mActivityTestRule.getActivity(), 2, UrlConstants.NTP_URL,
                true /* expectHomeSurfaceUiShown */);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    public void testStartSurfaceOnTabletWithNtpExist() throws IOException {
        // The existing NTP isn't the last active Tab.
        String modifiedNtpUrl = UrlConstants.NTP_URL + "/1";
        Assert.assertTrue(UrlUtilities.isNTPUrl(modifiedNtpUrl));
        StartSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(mActivityTestRule.getActivity(), 3, UrlConstants.NTP_URL,
                true /* expectHomeSurfaceUiShown */);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    public void testStartSurfaceOnTabletWithActiveNtpExist() throws IOException {
        // The existing NTP is set as the last active Tab.
        String modifiedNtpUrl = UrlConstants.NTP_URL + "/1";
        Assert.assertTrue(UrlUtilities.isNTPUrl(modifiedNtpUrl));
        StartSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 1);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that no new NTP is created, and the existing NTP is reused and set as the
        // current Tab.
        verifyTabCountAndActiveTabUrl(mActivityTestRule.getActivity(), 2, modifiedNtpUrl,
                false /* expectHomeSurfaceUiShown */);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    public void testScrollableMvTilesEnabledOnTablet() throws IOException {
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        // Verifies that a NTP is created and set as the current Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 2, UrlConstants.NTP_URL, true /* expectHomeSurfaceUiShown */);

        waitForNtpLoaded(cta.getActivityTab());
        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup mvTilesLayout =
                ntp.getView().findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);
        // Verifies that 1 row MV tiles are shown when "Start surface on tablet" flag is enabled.
        Assert.assertTrue(mvTilesLayout instanceof MostVisitedTilesCarouselLayout);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID})
    @DisableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET})
    public void testScrollableMvTilesDefaultDisabledOnTablet() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup mvTilesLayout =
                ntp.getView().findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);
        // Verifies that 2 row MV tiles are shown when "Start surface on tablet" flag is disabled.
        Assert.assertTrue(mvTilesLayout instanceof MostVisitedTilesGridLayout);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    public void testSingleTabCardGoneAfterTabClosed() throws IOException {
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 2, UrlConstants.NTP_URL, true /* expectHomeSurfaceUiShown */);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        Assert.assertTrue(ntp.isSingleTabCardVisibleForTesting());

        Tab lastActiveTab = cta.getCurrentTabModel().getTabAt(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getCurrentTabModel().closeTab(lastActiveTab); });
        Assert.assertEquals(1, cta.getCurrentTabModel().getCount());
        Assert.assertFalse(ntp.isSingleTabCardVisibleForTesting());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID,
            ChromeFeatureList.FEED_MULTI_COLUMN, ChromeFeatureList.START_SURFACE_ON_TABLET})
    // clang-format off
    public void testFakeSearchBoxWidthShortenedWith1RowMvTitlesAndMultiColumnFeeds() {
        // clang-format on
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup mvTilesLayout =
                ntp.getView().findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);
        // Verifies that 1 row MV tiles are shown when "Start surface on tablet" flag is enabled.
        Assert.assertTrue(mvTilesLayout instanceof MostVisitedTilesCarouselLayout);

        Resources res = cta.getResources();
        int expectedTwoSideMarginPortrait =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.tile_grid_layout_bleed);
        int expectedTwoSideMarginLandscape =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.ntp_search_box_start_margin)
                        * 2
                + expectedTwoSideMarginPortrait;

        // Verifies there is additional margin added for the fake search box in landscape mode,
        // but not in the portrait mode.
        verifyFakeSearchBoxWidth(
                expectedTwoSideMarginLandscape, expectedTwoSideMarginPortrait, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID,
            ChromeFeatureList.START_SURFACE_ON_TABLET})
    @DisableFeatures({ChromeFeatureList.FEED_MULTI_COLUMN})
    // clang-format off
    public void testFakeSearchBoxWidthNotChangeWith1RowMvTitlesAndMultiColumnFeedsDisabled() {
        // clang-format on
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup mvTilesLayout =
                ntp.getView().findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);
        // Verifies that 1 row MV tiles are shown when "Start surface on tablet" flag is enabled.
        Assert.assertTrue(mvTilesLayout instanceof MostVisitedTilesCarouselLayout);

        Resources res = cta.getResources();
        int expectedMargin =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.tile_grid_layout_bleed);

        // Verifies there isn't additional margin added for the fake search box in bot landscape and
        // portrait mode when multiple Column Feeds is disabled.
        verifyFakeSearchBoxWidth(expectedMargin, expectedMargin, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID})
    @EnableFeatures(
            {ChromeFeatureList.START_SURFACE_ON_TABLET, ChromeFeatureList.FEED_MULTI_COLUMN})
    // clang-format off
    public void testFakeSearchBoxWidthShortenedWith2RowMvTitlesAndMultiColumnFeeds() {
        // clang-format on
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup mvTilesLayout =
                ntp.getView().findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);
        // Verifies that 2 row MV tiles are shown when "Start surface on tablet" flag is disabled.
        Assert.assertTrue(mvTilesLayout instanceof MostVisitedTilesGridLayout);

        Resources res = cta.getResources();
        int expectedTwoSideMargin =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.ntp_search_box_start_margin)
                        * 2
                + res.getDimensionPixelSize(org.chromium.chrome.R.dimen.tile_grid_layout_bleed);

        // Verifies there is additional margin added for the fake search box in both landscape
        // and portrait modes.
        verifyFakeSearchBoxWidth(expectedTwoSideMargin, expectedTwoSideMargin, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID,
            ChromeFeatureList.FEED_MULTI_COLUMN})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET})
    // clang-format off
    public void testFakeSearchBoxWidthNotChangeWith2RowMvTitlesAndMultiColumnFeedsDisabled() {
        // clang-format on
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup mvTilesLayout =
                ntp.getView().findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);
        // Verifies that 2 row MV tiles are shown when "Start surface on tablet" flag is disabled.
        Assert.assertTrue(mvTilesLayout instanceof MostVisitedTilesGridLayout);

        Resources res = cta.getResources();
        int expectedMargin =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.tile_grid_layout_bleed);

        // Verifies there isn't additional margin added for the fake search box in bot landscape and
        // portrait mode when multiple Column Feeds is disabled.
        verifyFakeSearchBoxWidth(expectedMargin, expectedMargin, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET})
    // clang-format off
    public void testLogoSizeShrink() {
        // clang-format on
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup logoView = ntp.getView().findViewById(R.id.search_provider_logo);

        Resources res = cta.getResources();
        int expectedLogoHeight = res.getDimensionPixelSize(R.dimen.ntp_logo_height_shrink);
        int expectedVerticalMargin =
                res.getDimensionPixelSize(R.dimen.ntp_logo_vertical_margin_tablet);

        // Verifies the logo size is decreased, and top bottom margins are updated.
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        Assert.assertEquals(expectedLogoHeight, marginLayoutParams.height);
        Assert.assertEquals(expectedVerticalMargin, marginLayoutParams.topMargin);
        Assert.assertEquals(expectedVerticalMargin, marginLayoutParams.bottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET})
    // clang-format off
    public void testDefaultLogoSize() {
        // clang-format on
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup logoView = ntp.getView().findViewById(R.id.search_provider_logo);

        Resources res = cta.getResources();
        int expectedLogoHeight = res.getDimensionPixelSize(R.dimen.ntp_logo_height);
        int expectedMarginTop = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int expectedMarginBottom = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);

        // Verifies logo has its original size and margins.
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        Assert.assertEquals(expectedLogoHeight, marginLayoutParams.height);
        Assert.assertEquals(expectedMarginTop, marginLayoutParams.topMargin);
        Assert.assertEquals(expectedMarginBottom, marginLayoutParams.bottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    @DisableFeatures({ChromeFeatureList.FEED_MULTI_COLUMN})
    // clang-format off
    public void testDefaultSingleTabViewMargin() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 2, UrlConstants.NTP_URL, true /* expectHomeSurfaceUiShown */);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        View singleTabView = ntp.getView().findViewById(R.id.single_tab_view);

        Resources res = cta.getResources();
        int defaultLateralMargin =
                res.getDimensionPixelSize(R.dimen.single_tab_card_lateral_margin);

        // Verifies that the single Tab card has its original margins.
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) singleTabView.getLayoutParams();
        Assert.assertEquals(defaultLateralMargin, marginLayoutParams.getMarginStart());
        Assert.assertEquals(defaultLateralMargin, marginLayoutParams.getMarginEnd());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID,
            ChromeFeatureList.FEED_MULTI_COLUMN, ChromeFeatureList.START_SURFACE_ON_TABLET})
    // clang-format off
    public void test1RowMvtMarginWithMultiColumnFeedsOnNtpHomePage() throws IOException{
        // clang-format on
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        Resources res = cta.getResources();

        int expectedContainerTwoSideMarginLandscape =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.ntp_search_box_start_margin)
                        * 2
                + res.getDimensionPixelSize(
                        org.chromium.chrome.R.dimen
                                .mvt_container_to_ntp_right_extra_margin_two_feed_tablet)
                + res.getDimensionPixelSize(org.chromium.chrome.R.dimen.tile_grid_layout_bleed);
        int expectedContainerTwoSideMarginPortrait =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.tile_grid_layout_bleed)
                + res.getDimensionPixelSize(
                        org.chromium.chrome.R.dimen
                                .mvt_container_to_ntp_right_extra_margin_two_feed_tablet);
        // Verifies the margins of the module most visited tiles and its inner view are correct.
        verifyMostVisitedTileMargin(expectedContainerTwoSideMarginLandscape,
                expectedContainerTwoSideMarginPortrait, 0, 0, /*isScrollable=*/true, ntp);

        int expectedMvtBottomMargin = res.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.mvt_container_bottom_margin_tablet);
        int expectedSingleTabCardTopAndBottomMargin = res.getDimensionPixelSize(
                org.chromium.chrome.R.dimen
                        .single_tab_card_top_and_bottom_margin_carousel_mvt_tablet);
        // Verifies the vertical margins of the module most visited tiles and single tab card are
        // correct.
        verifyMvtAndSingleTabCardVerticalMargins(expectedMvtBottomMargin,
                -expectedSingleTabCardTopAndBottomMargin, expectedSingleTabCardTopAndBottomMargin,
                /*isNtpHomepage=*/true, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID,
            ChromeFeatureList.FEED_MULTI_COLUMN, ChromeFeatureList.START_SURFACE_ON_TABLET})
    // clang-format off
    public void test1RowMvtMarginWithMultiColumnFeedsOnEmptyNtp() {
        // clang-format on
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        Resources res = cta.getResources();

        int expectedMvtBottomMargin = res.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.mvt_container_bottom_margin_tablet);
        // Verifies the vertical margins of the module most visited tiles is correct.
        verifyMvtAndSingleTabCardVerticalMargins(
                expectedMvtBottomMargin, 0, 0, /*isNtpHomepage=*/false, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    @EnableFeatures(
            {ChromeFeatureList.FEED_MULTI_COLUMN, ChromeFeatureList.START_SURFACE_ON_TABLET})
    @DisableFeatures(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID)
    // clang-format off
    public void test2RowMvtMarginWithMultiColumnFeedsOnNtpHomePage() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        Resources res = cta.getResources();

        int expectedContainerTwoSideMargin =
                res.getDimensionPixelSize(org.chromium.chrome.R.dimen.ntp_search_box_start_margin)
                        * 2
                + res.getDimensionPixelSize(org.chromium.chrome.R.dimen.tile_grid_layout_bleed);
        int expectedLandScapeEdgeMargin = res.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_grid_layout_landscape_edge_margin_tablet);
        int expectedPortraitEdgeMargin = res.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_grid_layout_portrait_edge_margin_tablet);
        // Verifies the margins of the module most visited tiles and its inner view are correct.
        verifyMostVisitedTileMargin(expectedContainerTwoSideMargin, expectedContainerTwoSideMargin,
                expectedLandScapeEdgeMargin, expectedPortraitEdgeMargin, /*isScrollable=*/false,
                ntp);

        int expectedMvtBottomMargin = res.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.mvt_container_bottom_margin_tablet);
        int expectedSingleTabCardTopAndBottomMargin = res.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.single_tab_card_top_and_bottom_margin_grid_mvt_tablet);
        // Verifies the vertical margins of the module most visited tiles and single tab card are
        // correct.
        verifyMvtAndSingleTabCardVerticalMargins(expectedMvtBottomMargin,
                -expectedSingleTabCardTopAndBottomMargin, expectedSingleTabCardTopAndBottomMargin,
                /*isNtpHomepage=*/true, ntp);
    }

    /**
     * Verifies the margins of the module most visited tiles and its inner view are correct when it
     * appears on a tablet.
     * @param expectedContainerTwoSideMarginLandScape The expected sum of two side margins of the
     *        most visited tiles container when the tablet is in landscape.
     * @param expectedContainerTwoSideMarginPortrait The expected sum of two side margins of the
     *        most visited tiles container when the tablet is in portrait.
     * @param expectedEdgeMarginLandScape The expected edge margin of the most visited tiles element
     *        to the MV tiles layout when the tablet is in landscape.
     * @param expectedEdgeMarginPortrait The expected edge margin of the most visited tiles element
     *        to the MV tiles layout when the tablet is in portrait.
     * @param isScrollable Whether the most visited tiles is scrollable.
     * @param ntp The current {@link NewTabPage}.
     */
    private void verifyMostVisitedTileMargin(int expectedContainerTwoSideMarginLandScape,
            int expectedContainerTwoSideMarginPortrait, int expectedEdgeMarginLandScape,
            int expectedEdgeMarginPortrait, boolean isScrollable, NewTabPage ntp) {
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();
        View mvTilesContainer =
                ntpLayout.findViewById(org.chromium.chrome.test.R.id.mv_tiles_container);
        View mvTilesLayout = ntpLayout.findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);
        View mvTileItem1 = ((ViewGroup) mvTilesLayout).getChildAt(0);
        View mvTileItem2 = ((ViewGroup) mvTilesLayout).getChildAt(1);
        int mvTilesItemWidth = mvTileItem1.getWidth();

        // Start off in landscape screen orientation.
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForScreenOrientation("\"landscape\"");
        // Verifies the margins added for the most visited tiles are correct.
        verifyMostVisitedTileMarginImpl(ntpLayout, mvTilesContainer, mvTilesLayout, mvTileItem1,
                mvTileItem2, expectedContainerTwoSideMarginLandScape, expectedEdgeMarginLandScape,
                mvTilesItemWidth, isScrollable);

        // Start off in portrait screen orientation.
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForScreenOrientation("\"portrait\"");
        // Verifies the margins added for the most visited tiles are correct.
        verifyMostVisitedTileMarginImpl(ntpLayout, mvTilesContainer, mvTilesLayout, mvTileItem1,
                mvTileItem2, expectedContainerTwoSideMarginPortrait, expectedEdgeMarginPortrait,
                mvTilesItemWidth, isScrollable);
    }

    /**
     * Verifies the margins of the module most visited tiles and its inner view are correct when it
     * appears on a tablet.
     * @param ntpLayout The current {@link NewTabPageLayout}.
     * @param mvTilesContainer The container for the most visited tile.
     * @param mvTilesLayout The current {@link MostVisitedTilesLayout}.
     * @param mvTileItem1 The initial element of the most visited tile.
     * @param mvTileItem2 The second element of the most visited tile.
     * @param expectedContainerTwoSideMargin The expected sum of two side margins of the
     *                                       most visited tiles container.
     * @param expectedEdgeMargin The expected edge margin of the most visited tiles element
     *                           to the MV tiles layout.
     * @param mvTilesItemWidth The width of the elements in the most visited tile.
     * @param isScrollable Whether the most visited tiles is scrollable.
     */
    private void verifyMostVisitedTileMarginImpl(View ntpLayout, View mvTilesContainer,
            View mvTilesLayout, View mvTileItem1, View mvTileItem2,
            int expectedContainerTwoSideMargin, int expectedEdgeMargin, int mvTilesItemWidth,
            boolean isScrollable) {
        int mvtContainerWidth = mvTilesContainer.getWidth();
        int mvTilesLayoutWidth = mvTilesLayout.getWidth();
        int mvt1LeftMargin = ((MarginLayoutParams) mvTileItem1.getLayoutParams()).leftMargin;
        int mvt2LeftMargin = ((MarginLayoutParams) mvTileItem2.getLayoutParams()).leftMargin;

        Assert.assertEquals("The container's margin with respect to the layout of the new tab "
                        + "page is incorrect.",
                expectedContainerTwoSideMargin, ntpLayout.getWidth() - mvtContainerWidth);

        if (isScrollable) {
            Assert.assertTrue("The width of the most visited tiles layout is wrong.",
                    mvtContainerWidth < mvTilesLayoutWidth);
            int mvtWithPadding = mvTilesItemWidth + mvt2LeftMargin;
            int visibleMvtNum = mvtContainerWidth / mvtWithPadding;
            Assert.assertEquals("It fails to meet the requirement that half of "
                            + "the most visited tiles element should be at the end of the MV tiles "
                            + "when the new tab page is initially loaded.",
                    mvtContainerWidth - visibleMvtNum * mvtWithPadding, mvTilesItemWidth / 2,
                    mvTilesItemWidth / 20);
        } else {
            Assert.assertTrue("The width of the most visited tiles layout is wrong.",
                    mvtContainerWidth == mvTilesLayoutWidth);
            Assert.assertEquals("The edge margin of the most visited tiles element to "
                            + "the MV tiles layout is wrong.",
                    mvt1LeftMargin, expectedEdgeMargin);
            int expectedIntervalPadding =
                    (mvTilesLayoutWidth - mvTilesItemWidth * 4 - expectedEdgeMargin * 2) / 3;
            Assert.assertEquals(
                    "The padding between each element of the most visited tiles is incorrect.",
                    expectedIntervalPadding,
                    mvt2LeftMargin - mvTilesItemWidth - expectedEdgeMargin);
        }
    }

    /**
     * Verifies the vertical margins of the module most visited tiles and single tab card are
     * correct when they appear on a tablet.
     * @param expectedMvtBottomMargin The expected bottom margin of the most visited tile.
     * @param expectedSingleTabCardTopMargin The expected top margin of the Single Tab Card
     *         container.
     * @param expectedSingleTabCardBottomMargin The expected bottom margin of the Single Tab Card
     *                                          container.
     * @param isNtpHomepage Whether the current new tab page is shown as the homepage.
     * @param ntp The current {@link NewTabPage}.
     */
    private void verifyMvtAndSingleTabCardVerticalMargins(int expectedMvtBottomMargin,
            int expectedSingleTabCardTopMargin, int expectedSingleTabCardBottomMargin,
            boolean isNtpHomepage, NewTabPage ntp) {
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();
        View mvTilesContainer =
                ntpLayout.findViewById(org.chromium.chrome.test.R.id.mv_tiles_container);
        Assert.assertEquals("The bottom margin of the most visited tiles container is wrong.",
                expectedMvtBottomMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).bottomMargin);
        if (isNtpHomepage) {
            View singleTabCardContainer = ntpLayout.findViewById(
                    org.chromium.chrome.test.R.id.tab_switcher_module_container);
            MarginLayoutParams singleTabCardContainerMarginParams =
                    (MarginLayoutParams) singleTabCardContainer.getLayoutParams();
            Assert.assertEquals("The top margin of the single tab card container is wrong.",
                    expectedSingleTabCardTopMargin, singleTabCardContainerMarginParams.topMargin);
            Assert.assertEquals("The bottom margin of the single tab card container is wrong.",
                    expectedSingleTabCardBottomMargin,
                    singleTabCardContainerMarginParams.bottomMargin);
        }
    }

    private void verifyTabCountAndActiveTabUrl(
            ChromeTabbedActivity cta, int tabCount, String url, Boolean expectHomeSurfaceUiShown) {
        Assert.assertEquals(tabCount, cta.getCurrentTabModel().getCount());
        Tab tab = StartSurfaceTestUtils.getCurrentTabFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(TextUtils.equals(url, tab.getUrl().getSpec())); });
        if (expectHomeSurfaceUiShown != null) {
            Assert.assertEquals(expectHomeSurfaceUiShown,
                    ((NewTabPage) tab.getNativePage()).isSingleTabCardVisibleForTesting());
        }
    }

    private static void waitForNtpLoaded(final Tab tab) {
        assert !tab.isIncognito();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tab.getNativePage(), Matchers.instanceOf(NewTabPage.class));
            Criteria.checkThat(
                    ((NewTabPage) tab.getNativePage()).isLoadedForTests(), Matchers.is(true));
        });
    }

    private void verifyFakeSearchBoxWidth(
            int expectedLandScapeWidth, int expectedPortraitWidth, NewTabPage ntp) {
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();
        View searchBoxLayout = ntpLayout.findViewById(org.chromium.chrome.test.R.id.search_box);

        // Start off in landscape screen orientation.
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForScreenOrientation("\"landscape\"");
        // Verifies there is additional margins added for the fake search box.
        Assert.assertEquals(
                expectedLandScapeWidth, ntpLayout.getWidth() - searchBoxLayout.getWidth());

        // Start off in portrait screen orientation.
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForScreenOrientation("\"portrait\"");
        // Verifies there is additional margins added for the fake search box.
        Assert.assertEquals(
                expectedPortraitWidth, ntpLayout.getWidth() - searchBoxLayout.getWidth());
    }

    private void waitForScreenOrientation(String orientationValue) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                Criteria.checkThat(screenOrientation(), Matchers.is(orientationValue));
            } catch (TimeoutException ex) {
                throw new CriteriaNotSatisfiedException(ex);
            }
        });
    }

    private String screenOrientation() throws TimeoutException {
        // Returns "\"portrait\"" or "\"landscape\"" (strips the "-primary" or "-secondary" suffix).
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), "screen.orientation.type.split('-')[0]");
    }
}
