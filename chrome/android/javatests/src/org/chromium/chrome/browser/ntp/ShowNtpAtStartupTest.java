// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.ntp.HomeSurfaceTestUtils.IMMEDIATE_RETURN_TEST_PARAMS;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_AT_STARTUP_UMA;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_UMA;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.pm.ActivityInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.logo.LogoView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/** Integration tests of showing a NTP with Start surface UI at startup. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study"})
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@DoNotBatch(reason = "This test suite tests startup behaviors.")
public class ShowNtpAtStartupTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TAB_URL = "https://foo.com/";
    private static final String TAB_URL_1 = "https://bar.com/";

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testShowNtpAtStartup() throws IOException {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        HomeSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that a NTP is created and set as the current Tab.
        verifyTabCountAndActiveTabUrl(
                mActivityTestRule.getActivity(),
                2,
                UrlConstants.NTP_URL,
                /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());

        histogram.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testShowNtpAtStartupWithNtpExist() throws IOException {
        // The existing NTP isn't the last active Tab.
        String modifiedNtpUrl = UrlConstants.NTP_URL + "/1";
        Assert.assertTrue(UrlUtilities.isNtpUrl(modifiedNtpUrl));

        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 0);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        HomeSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                mActivityTestRule.getActivity(),
                3,
                UrlConstants.NTP_URL,
                /* expectHomeSurfaceUiShown= */ true);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testShowNtpAtStartupWithActiveNtpExist() throws IOException {
        // The existing NTP is set as the last active Tab.
        String modifiedNtpUrl = UrlConstants.NTP_URL + "/1";
        Assert.assertTrue(UrlUtilities.isNtpUrl(modifiedNtpUrl));
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();

        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 1);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        HomeSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that no new NTP is created, and the existing NTP is reused and set as the
        // current Tab.
        verifyTabCountAndActiveTabUrl(
                mActivityTestRule.getActivity(),
                2,
                modifiedNtpUrl,
                /* expectHomeSurfaceUiShown= */ false);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.MAGIC_STACK_ANDROID + "<Study"})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testSingleTabCardGoneAfterTabClosed_MagicStack() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, TAB_URL_1}, 0);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 3, UrlConstants.NTP_URL, /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        Assert.assertTrue(ntp.isMagicStackVisibleForTesting());
        View singleTabModule = cta.findViewById(R.id.single_tab_view);
        Assert.assertNotNull(singleTabModule.findViewById(R.id.tab_thumbnail));

        // Verifies that closing the tracking Tab will remove the "continue browsing" card from
        // the NTP.
        Tab lastActiveTab = cta.getCurrentTabModel().getTabAt(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getCurrentTabModel()
                            .closeTabs(
                                    TabClosureParams.closeTab(lastActiveTab)
                                            .allowUndo(false)
                                            .build());
                });
        Assert.assertEquals(2, cta.getCurrentTabModel().getCount());
        Assert.assertFalse(ntp.isMagicStackVisibleForTesting());

        // Tests to set another tracking Tab on the NTP.
        Tab newTrackingTab = cta.getCurrentTabModel().getTabAt(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ntp.showMagicStack(newTrackingTab);
                });
        CriteriaHelper.pollUiThread(() -> ntp.isMagicStackVisibleForTesting());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getCurrentTabModel()
                            .closeTabs(
                                    TabClosureParams.closeTab(newTrackingTab)
                                            .allowUndo(false)
                                            .build());
                });
        Assert.assertEquals(1, cta.getCurrentTabModel().getCount());
        Assert.assertFalse(ntp.isMagicStackVisibleForTesting());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testSingleTabModule() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, TAB_URL_1}, 0);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 3, UrlConstants.NTP_URL, /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        Assert.assertTrue(ntp.isMagicStackVisibleForTesting());
        onViewWaiting(allOf(withId(R.id.single_tab_view), isDisplayed()));
        View singleTabModule = cta.findViewById(R.id.single_tab_view);
        Assert.assertEquals(
                View.VISIBLE, singleTabModule.findViewById(R.id.tab_thumbnail).getVisibility());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.MAGIC_STACK_ANDROID + "<Study"})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testSingleTabModule_MagicStack() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, TAB_URL_1}, 0);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 3, UrlConstants.NTP_URL, /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(cta.getActivityTab());

        onViewWaiting(allOf(withId(R.id.home_modules_recycler_view), isDisplayed()));
        View singleTabModule = cta.findViewById(R.id.single_tab_view);
        Assert.assertEquals(
                View.VISIBLE, singleTabModule.findViewById(R.id.tab_thumbnail).getVisibility());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    public void testNtpLogoSize() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        Resources res = mActivityTestRule.getActivity().getResources();
        int expectedLogoHeight = res.getDimensionPixelSize(R.dimen.ntp_logo_height);
        int expectedTopMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int expectedBottomMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);

        // Verifies the logo size is decreased, and top bottom margins are updated.
        testLogoSizeImpl(expectedLogoHeight, expectedTopMargin, expectedBottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    public void testNtpDoodleSize() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        LogoView logoView = ntp.getView().findViewById(R.id.search_provider_logo);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Logo logo =
                            new Logo(Bitmap.createBitmap(1, 1, Config.ALPHA_8), null, null, null);
                    logoView.updateLogo(logo);
                    logoView.endAnimationsForTesting();
                });

        Resources res = mActivityTestRule.getActivity().getResources();
        int expectedLogoHeight = LogoUtils.getLogoHeightForLogoPolishWithMediumSize(res);
        int expectedTopMargin = LogoUtils.getTopMarginForLogoPolish(res);
        int expectedBottomMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);

        // Verifies the logo size is decreased, and top bottom margins are updated.
        testLogoSizeImpl(expectedLogoHeight, expectedTopMargin, expectedBottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction({DeviceFormFactor.TABLET})
    public void testMvtAndSingleTabCardVerticalMargin() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();

        // Verifies the vertical margins of the module most visited tiles is correct.
        verifyMvtAndSingleTabCardVerticalMargins(
                /* expectedMvtBottomMargin= */ 0,
                /* expectedSingleTabCardTopMargin= */ 0,
                /* expectedSingleTabCardBottomMargin= */ 0,
                /* isNtpHomepage= */ false,
                ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({
        ChromeFeatureList.MAGIC_STACK_ANDROID,
    })
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testClickSingleTabCardCloseNtpHomeSurface() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 2, UrlConstants.NTP_URL, /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(cta.getActivityTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.findViewById(R.id.single_tab_view).performClick());

        // Verifies that the last active Tab is showing, and NTP home surface is closed.
        verifyTabCountAndActiveTabUrl(cta, 1, TAB_URL, /* expectHomeSurfaceUiShown= */ null);
    }

    private void testLogoSizeImpl(
            int expectedLogoHeight, int expectedTopMargin, int expectedBottomMargin) {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();
        ViewGroup logoView = ntp.getView().findViewById(R.id.search_provider_logo);

        // Verifies the logo size and margins.
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        Assert.assertEquals(expectedLogoHeight, marginLayoutParams.height);
        Assert.assertEquals(expectedTopMargin, marginLayoutParams.topMargin);
        Assert.assertEquals(expectedBottomMargin, marginLayoutParams.bottomMargin);
    }

    /**
     * Test the close of the tab to track for the single tab card on the {@link NewTabPage} in the
     * tablet.
     */
    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    @DisabledTest(message = "b/353758883")
    public void testThumbnailRecaptureForSingleTabCardAfterMostRecentTabClosed()
            throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        HomeSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 2, UrlConstants.NTP_URL, /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(cta.getActivityTab());

        Tab lastActiveTab = cta.getCurrentTabModel().getTabAt(0);
        Tab ntpTab = cta.getActivityTab();
        NewTabPage ntp = (NewTabPage) ntpTab.getNativePage();
        Assert.assertTrue(
                "The single tab card is still invisible after initialization.",
                ntp.isMagicStackVisibleForTesting());
        assertFalse(
                "There is a wrong signal that the single tab card is changed and needs a "
                        + "snapshot for the NTP.",
                ntp.getSnapshotSingleTabCardChangedForTesting());

        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.findViewById(R.id.tab_switcher_button).performClick());
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getTabModelSelector()
                            .getModel(false)
                            .closeTabs(
                                    TabClosureParams.closeTab(lastActiveTab)
                                            .allowUndo(false)
                                            .build());
                });
        assertTrue(
                "The single tab card does not show that it is changed and needs a "
                        + "snapshot for the NTP.",
                ntp.getSnapshotSingleTabCardChangedForTesting());

        ThreadUtils.runOnUiThreadBlocking(() -> cta.onBackPressed());
        NewTabPageTestUtils.waitForNtpLoaded(ntpTab);
        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.getLayoutManager().showLayout(LayoutType.TAB_SWITCHER, false));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        ThreadUtils.runOnUiThreadBlocking(() -> cta.onBackPressed());
        NewTabPageTestUtils.waitForNtpLoaded(ntpTab);
        assertFalse(
                "There is no extra snapshot for the NTP to cache the change "
                        + "of the single tab card.",
                ntp.getSnapshotSingleTabCardChangedForTesting());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction({DeviceFormFactor.TABLET})
    public void testFakeSearchBoxWidth() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();

        Resources res = cta.getResources();
        int expectedTwoSideMargin =
                2 * res.getDimensionPixelSize(R.dimen.ntp_search_box_lateral_margin_tablet);

        // Verifies there is additional margin added for the fake search box.
        verifyFakeSearchBoxWidth(expectedTwoSideMargin, expectedTwoSideMargin, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction({DeviceFormFactor.TABLET})
    @CommandLineFlags.Add({IMMEDIATE_RETURN_TEST_PARAMS})
    public void testMvtLayoutHorizontalMargin() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(cta.getActivityTab());

        NewTabPage ntp = (NewTabPage) cta.getActivityTab().getNativePage();

        Resources res = cta.getResources();
        int expectedContainerTwoSideMargin = 0;
        int expectedMvtLayoutEdgeMargin =
                res.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
        int expectedMvtLayoutIntervalMargin =
                res.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet);

        verifyMostVisitedTileMargin(
                expectedContainerTwoSideMargin,
                expectedMvtLayoutEdgeMargin,
                expectedMvtLayoutIntervalMargin,
                ntp);
    }

    /**
     * Verifies the vertical margins of the module most visited tiles and single tab card are
     * correct when they appear on a tablet.
     *
     * @param expectedMvtBottomMargin The expected bottom margin of the most visited tile.
     * @param expectedSingleTabCardTopMargin The expected top margin of the Single Tab Card
     *     container.
     * @param expectedSingleTabCardBottomMargin The expected bottom margin of the Single Tab Card
     *     container.
     * @param isNtpHomepage Whether the current new tab page is shown as the homepage.
     * @param ntp The current {@link NewTabPage}.
     */
    private void verifyMvtAndSingleTabCardVerticalMargins(
            int expectedMvtBottomMargin,
            int expectedSingleTabCardTopMargin,
            int expectedSingleTabCardBottomMargin,
            boolean isNtpHomepage,
            NewTabPage ntp) {
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();
        View mvTilesContainer = ntpLayout.findViewById(R.id.mv_tiles_container);
        Assert.assertEquals(
                "The bottom margin of the most visited tiles container is wrong.",
                expectedMvtBottomMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).bottomMargin);
        verifySingleTabCardVerticalMargins(
                expectedSingleTabCardTopMargin,
                expectedSingleTabCardBottomMargin,
                isNtpHomepage,
                ntp);
    }

    private void verifySingleTabCardVerticalMargins(
            int expectedSingleTabCardTopMargin,
            int expectedSingleTabCardBottomMargin,
            boolean isNtpHomepage,
            NewTabPage ntp) {
        if (!isNtpHomepage) return;
        View singleTabCardContainer =
                ntp.getNewTabPageLayout().findViewById(R.id.tab_switcher_module_container);
        MarginLayoutParams singleTabCardContainerMarginParams =
                (MarginLayoutParams) singleTabCardContainer.getLayoutParams();
        Assert.assertEquals(
                "The top margin of the single tab card container is wrong.",
                expectedSingleTabCardTopMargin,
                singleTabCardContainerMarginParams.topMargin);
        Assert.assertEquals(
                "The bottom margin of the single tab card container is wrong.",
                expectedSingleTabCardBottomMargin,
                singleTabCardContainerMarginParams.bottomMargin);
    }

    private void verifyTabCountAndActiveTabUrl(
            ChromeTabbedActivity cta, int tabCount, String url, Boolean expectHomeSurfaceUiShown) {
        Assert.assertEquals(tabCount, cta.getCurrentTabModel().getCount());
        Tab tab = HomeSurfaceTestUtils.getCurrentTabFromUIThread(cta);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(TextUtils.equals(url, tab.getUrl().getSpec()));
                });
        if (expectHomeSurfaceUiShown != null) {
            Assert.assertEquals(
                    expectHomeSurfaceUiShown,
                    ((NewTabPage) tab.getNativePage()).isMagicStackVisibleForTesting());
        }
    }

    private static void waitForNtpLoaded(final Tab tab) {
        assert !tab.isIncognito();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(tab.getNativePage(), Matchers.instanceOf(NewTabPage.class));
                    Criteria.checkThat(
                            ((NewTabPage) tab.getNativePage()).isLoadedForTests(),
                            Matchers.is(true));
                });
    }

    private void verifyFakeSearchBoxWidth(
            int expectedLandScapeWidth, int expectedPortraitWidth, NewTabPage ntp) {
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();
        View searchBoxLayout = ntpLayout.findViewById(R.id.search_box);

        // Orientation changes are not supported on automotive.
        if (BuildInfo.getInstance().isAutomotive) {
            verifyFakeSearchBoxWidthForCurrentOrientation(
                    expectedLandScapeWidth, expectedPortraitWidth, ntpLayout, searchBoxLayout);
            return;
        }

        // Start off in landscape screen orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForScreenOrientation("\"landscape\"");
        // Verifies there is additional margins added for the fake search box.
        Assert.assertEquals(
                expectedLandScapeWidth, ntpLayout.getWidth() - searchBoxLayout.getWidth());

        // Start off in portrait screen orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForScreenOrientation("\"portrait\"");
        // Verifies there is additional margins added for the fake search box.
        Assert.assertEquals(
                expectedPortraitWidth, ntpLayout.getWidth() - searchBoxLayout.getWidth());
    }

    private void verifyFakeSearchBoxWidthForCurrentOrientation(
            int expectedLandScapeWidth,
            int expectedPortraitWidth,
            NewTabPageLayout ntpLayout,
            View searchBoxLayout) {
        int expectedWidth;
        try {
            String orientation = screenOrientation();
            if ("\"landscape\"".equals(orientation)) {
                expectedWidth = expectedLandScapeWidth;
            } else if ("\"portrait\"".equals(orientation)) {
                expectedWidth = expectedPortraitWidth;
            } else {
                throw new IllegalStateException(
                        "The device should either be in portrait or landscape mode.");
            }
        } catch (TimeoutException ex) {
            throw new CriteriaNotSatisfiedException(ex);
        }

        Assert.assertEquals(expectedWidth, ntpLayout.getWidth() - searchBoxLayout.getWidth());
    }

    private void verifyMostVisitedTileMargin(
            int expectedContainerWidth,
            int expectedEdgeMargin,
            int expectedIntervalMargin,
            NewTabPage ntp) {
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();
        View mvtContainer = ntpLayout.findViewById(R.id.mv_tiles_container);
        View mvTilesLayout = ntpLayout.findViewById(R.id.mv_tiles_layout);
        int mvt1LeftMargin =
                ((MarginLayoutParams) ((ViewGroup) mvTilesLayout).getChildAt(0).getLayoutParams())
                        .leftMargin;
        int mvt2LeftMargin =
                ((MarginLayoutParams) ((ViewGroup) mvTilesLayout).getChildAt(1).getLayoutParams())
                        .leftMargin;

        // Orientation changes are not supported on automotive.
        if (BuildInfo.getInstance().isAutomotive) {
            verifyTileMargin(
                    expectedContainerWidth,
                    expectedEdgeMargin,
                    expectedIntervalMargin,
                    ntpLayout,
                    mvtContainer,
                    mvt1LeftMargin,
                    mvt2LeftMargin);
            return;
        }

        // Start off in landscape screen orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForScreenOrientation("\"landscape\"");
        verifyTileMargin(
                expectedContainerWidth,
                expectedEdgeMargin,
                expectedIntervalMargin,
                ntpLayout,
                mvtContainer,
                mvt1LeftMargin,
                mvt2LeftMargin);

        // Start off in portrait screen orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForScreenOrientation("\"portrait\"");
        verifyTileMargin(
                expectedContainerWidth,
                expectedEdgeMargin,
                expectedIntervalMargin,
                ntpLayout,
                mvtContainer,
                mvt1LeftMargin,
                mvt2LeftMargin);
    }

    private void verifyTileMargin(
            int expectedContainerWidth,
            int expectedEdgeMargin,
            int expectedIntervalMargin,
            NewTabPageLayout ntpLayout,
            View mvtContainer,
            int mvt1LeftMargin,
            int mvt2LeftMargin) {
        // Verifies there is no additional margins added for the mv tiles container.
        Assert.assertEquals(expectedContainerWidth, ntpLayout.getWidth() - mvtContainer.getWidth());
        // Verifies the inner margins of the mv tiles module.
        assertTrue(mvt1LeftMargin >= expectedEdgeMargin);
        Assert.assertEquals(expectedIntervalMargin, mvt2LeftMargin);
    }

    private void waitForScreenOrientation(String orientationValue) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
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
