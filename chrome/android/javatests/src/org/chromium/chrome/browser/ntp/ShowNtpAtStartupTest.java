// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2;
import static org.chromium.chrome.browser.ntp.HomeSurfaceTestUtils.START_SURFACE_RETURN_TIME_IMMEDIATE;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_AT_STARTUP_UMA;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_UMA;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.educational_tip.EducationalTipModuleUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoContainerView;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.suggestions.tile.TilesLinearLayout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;

/** Integration tests of showing a NTP with Start surface UI at startup. */
@RunWith(ChromeJUnit4ClassRunner.class)
// Restrict to Phones and Tablets because Desktop Android does not show NTP at startup.
@Restriction(DeviceFormFactor.PHONE_OR_TABLET)
@EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME})
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(reason = "This test suite tests startup behaviors.")
public class ShowNtpAtStartupTest {
    private static final int RENDER_TEST_REVISION = 2;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

    private static final String TAB_URL = "https://foo.com/";
    private static final String TAB_URL_1 = "https://bar.com/";

    @Before
    public void setUp() {
        SetupListManager setupListManager = Mockito.mock(SetupListManager.class);
        Mockito.when(setupListManager.isSetupListActive()).thenReturn(false);
        SetupListManager.setInstanceForTesting(setupListManager);

        EducationalTipModuleUtils.setEducationalTipActiveForTesting(false);
        // TODO(https://crbug.com/454091341): Enable incognito mode on this test suite.
        IncognitoUtils.setEnabledForTesting(false);
    }

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures(START_SURFACE_RETURN_TIME_IMMEDIATE)
    public void testShowNtpAtStartup() throws IOException {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        mActivityTestRule.startFromLauncherAtNtp();
        HomeSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that a NTP is created and set as the current Tab.
        verifyTabCountAndActiveTabUrl(
                mActivityTestRule.getActivity(),
                2,
                getOriginalNativeNtpUrl(),
                /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        histogram.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures(START_SURFACE_RETURN_TIME_IMMEDIATE)
    public void testShowNtpAtStartupWithNtpExist() throws IOException {
        // The existing NTP isn't the last active Tab.
        String modifiedNtpUrl = getOriginalNativeNtpUrl() + "/1";
        Assert.assertTrue(UrlUtilities.isNtpUrl(modifiedNtpUrl));

        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 0);
        mActivityTestRule.startFromLauncherAtNtp();
        HomeSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                mActivityTestRule.getActivity(),
                3,
                getOriginalNativeNtpUrl(),
                /* expectHomeSurfaceUiShown= */ true);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures(START_SURFACE_RETURN_TIME_IMMEDIATE)
    public void testShowNtpAtStartupWithActiveNtpExist() throws IOException {
        // The existing NTP is set as the last active Tab.
        String modifiedNtpUrl = getOriginalNativeNtpUrl() + "/1";
        Assert.assertTrue(UrlUtilities.isNtpUrl(modifiedNtpUrl));
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();

        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 1);
        mActivityTestRule.startFromLauncherAtNtp();
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
    @EnableFeatures({START_SURFACE_RETURN_TIME_IMMEDIATE})
    public void testSingleTabCardGoneAfterTabClosed_MagicStack() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, TAB_URL_1}, 0);
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 3, getOriginalNativeNtpUrl(), /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        Assert.assertTrue(ntp.isMagicStackVisibleForTesting());
        View singleTabModule = cta.findViewById(R.id.single_tab_view);
        Assert.assertNotNull(singleTabModule.findViewById(R.id.tab_thumbnail));

        // Verifies that closing the tracking Tab will remove the "continue browsing" card from
        // the NTP.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab lastActiveTab = cta.getCurrentTabModel().getTabAt(0);
                    cta.getCurrentTabModel()
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(lastActiveTab)
                                            .allowUndo(false)
                                            .build(),
                                    /* allowDialog= */ false);
                });
        Assert.assertEquals(2, mActivityTestRule.tabsCount(false));
        Assert.assertFalse(ntp.isMagicStackVisibleForTesting());

        // Tests to set another tracking Tab on the NTP.
        Tab newTrackingTab =
                ThreadUtils.runOnUiThreadBlocking(() -> cta.getCurrentTabModel().getTabAt(0));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ntp.showHomeSurfaceUiOnNtp(newTrackingTab);
                });
        CriteriaHelper.pollUiThread(() -> ntp.isMagicStackVisibleForTesting());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getCurrentTabModel()
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(newTrackingTab)
                                            .allowUndo(false)
                                            .build(),
                                    /* allowDialog= */ false);
                });
        Assert.assertEquals(1, mActivityTestRule.tabsCount(false));
        Assert.assertFalse(ntp.isMagicStackVisibleForTesting());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures(START_SURFACE_RETURN_TIME_IMMEDIATE)
    public void testSingleTabModule() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, TAB_URL_1}, 0);
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 3, getOriginalNativeNtpUrl(), /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        Assert.assertTrue(ntp.isMagicStackVisibleForTesting());

        waitForView(
                cta,
                allOf(withId(R.id.tab_thumbnail), isDescendantOfA(withId(R.id.single_tab_view))));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({START_SURFACE_RETURN_TIME_IMMEDIATE})
    public void testSingleTabModule_MagicStack() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, TAB_URL_1}, 0);
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 3, getOriginalNativeNtpUrl(), /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        waitForView(cta, withId(R.id.home_modules_recycler_view));
        waitForView(
                cta,
                allOf(withId(R.id.tab_thumbnail), isDescendantOfA(withId(R.id.single_tab_view))));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    public void testNtpLogoSize() {
        mActivityTestRule.startOnNtp();
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
        mActivityTestRule.startOnNtp();

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        final int[] expectedValues = new int[3];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LogoContainerView logoView =
                            (LogoContainerView)
                                    ntp.getView().findViewById(R.id.logo_container_view);
                    Logo logo =
                            new Logo(
                                    /* image= */ Bitmap.createBitmap(1, 1, Config.ALPHA_8),
                                    /* darkImage= */ Bitmap.createBitmap(1, 1, Config.ARGB_8888),
                                    /* onClickUrl= */ null,
                                    /* altText= */ null,
                                    /* animatedLogoUrl= */ null,
                                    /* darkAnimatedLogoUrl= */ null,
                                    /* logUrl= */ null);
                    logoView.updateLogo(logo);
                    logoView.endAnimationsForTesting();

                    Resources res = cta.getResources();
                    expectedValues[0] = LogoUtils.getDoodleHeight(res);
                    expectedValues[1] = LogoUtils.getTopMarginForDoodle(res);
                    expectedValues[2] = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);
                });

        int expectedLogoHeight = expectedValues[0];
        int expectedTopMargin = expectedValues[1];
        int expectedBottomMargin = expectedValues[2];

        // Verifies the logo size is decreased, and top bottom margins are updated.
        testLogoSizeImpl(expectedLogoHeight, expectedTopMargin, expectedBottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testMvtAndSingleTabCardVerticalMargin() {
        mActivityTestRule.startOnNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();

        // Verifies the vertical margins of the module most visited tiles is correct.
        verifyMvtAndSingleTabCardVerticalMargins(
                /* expectedMvtBottomMargin= */ cta.getResources()
                        .getDimensionPixelSize(R.dimen.ntp_section_bottom_margin),
                /* expectedSingleTabCardTopMargin= */ 0,
                /* expectedSingleTabCardBottomMargin= */ 0,
                /* isNtpHomepage= */ false,
                ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({START_SURFACE_RETURN_TIME_IMMEDIATE})
    public void testClickSingleTabCardCloseNtpHomeSurface() throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 2, getOriginalNativeNtpUrl(), /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.findViewById(R.id.single_tab_view).performClick());

        // Verifies that the last active Tab is showing, and NTP home surface is closed.
        verifyTabCountAndActiveTabUrl(cta, 1, TAB_URL, /* expectHomeSurfaceUiShown= */ null);
    }

    private void testLogoSizeImpl(
            int expectedLogoHeight, int expectedTopMargin, int expectedBottomMargin) {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        View logoContainerView = ntp.getView().findViewById(R.id.logo_container_view);
        View logoView = ntp.getView().findViewById(R.id.search_provider_logo);

        MarginLayoutParams logoContainerLayoutParams =
                (MarginLayoutParams) logoContainerView.getLayoutParams();
        MarginLayoutParams logoViewLayoutParams = (MarginLayoutParams) logoView.getLayoutParams();

        Assert.assertEquals(expectedLogoHeight, logoViewLayoutParams.height);
        Assert.assertEquals(expectedTopMargin, logoViewLayoutParams.topMargin);
        Assert.assertEquals(expectedBottomMargin, logoContainerLayoutParams.bottomMargin);
    }

    /**
     * Test the close of the tab to track for the single tab card on the {@link NewTabPage} in the
     * tablet.
     */
    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(START_SURFACE_RETURN_TIME_IMMEDIATE)
    @DisabledTest(message = "b/353758883")
    public void testThumbnailRecaptureForSingleTabCardAfterMostRecentTabClosed()
            throws IOException {
        HomeSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        // Verifies that a new NTP is created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(
                cta, 2, getOriginalNativeNtpUrl(), /* expectHomeSurfaceUiShown= */ true);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        Tab lastActiveTab = cta.getCurrentTabModel().getTabAt(0);
        Tab ntpTab = mActivityTestRule.getActivityTab();
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
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.HUB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getTabModelSelector()
                            .getModel(false)
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(lastActiveTab)
                                            .allowUndo(false)
                                            .build(),
                                    /* allowDialog= */ false);
                });
        assertTrue(
                "The single tab card does not show that it is changed and needs a "
                        + "snapshot for the NTP.",
                ntp.getSnapshotSingleTabCardChangedForTesting());

        ThreadUtils.runOnUiThreadBlocking(() -> cta.onBackPressed());
        NewTabPageTestUtils.waitForNtpLoaded(ntpTab);
        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.getLayoutManager().showLayout(LayoutType.HUB, false));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.HUB);
        ThreadUtils.runOnUiThreadBlocking(() -> cta.onBackPressed());
        NewTabPageTestUtils.waitForNtpLoaded(ntpTab);
        assertFalse(
                "There is no extra snapshot for the NTP to cache the change "
                        + "of the single tab card.",
                ntp.getSnapshotSingleTabCardChangedForTesting());
    }

    private View getNtpLayout() {
        return ((NewTabPage) mActivityTestRule.getActivityTab().getNativePage()).getLayout();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface", "RenderTest"})
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures({START_SURFACE_RETURN_TIME_IMMEDIATE})
    public void testFakeSearchBoxWidth_phones() throws IOException {
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        View searchBoxLayout = getNtpLayout().findViewById(R.id.search_box);

        // Orientation changes are not supported on automotive.
        if (DeviceInfo.isAutomotive()) {
            mRenderTestRule.render(searchBoxLayout, "ntp_search_box_automotive_v2");
            return;
        }

        // Start off in landscape screen orientation.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);

        // Re-fetch view to avoid potential staleness after orientation change.
        mRenderTestRule.render(
                getNtpLayout().findViewById(R.id.search_box), "ntp_search_box_landscape_v2");

        // Switch to portrait screen orientation.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);

        // Re-fetch view to avoid potential staleness after orientation change.
        mRenderTestRule.render(
                getNtpLayout().findViewById(R.id.search_box), "ntp_search_box_portrait_v2");
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testFakeSearchBoxWidth() {
        mActivityTestRule.startOnNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();

        verifyFakeSearchBoxWidth();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @EnableFeatures(START_SURFACE_RETURN_TIME_IMMEDIATE)
    public void testMvtLayoutHorizontalMargin() {
        mActivityTestRule.startOnNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();

        verifyMostVisitedTileMargin();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface", "RenderTest"})
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures({START_SURFACE_RETURN_TIME_IMMEDIATE, NEW_TAB_PAGE_CUSTOMIZATION_V2})
    // TODO(crbug.com/475816843): Remove this and update goldens once migration is complete.
    @DisableFeatures({SigninFeatures.SIGNIN_LEVEL_UP_BUTTON})
    public void testToolbar_defaultBackground() throws IOException {
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        View toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        assertEquals(0, toolbar.getPaddingTop());
        mRenderTestRule.render(toolbar, "ntp_toolbar_default_background");
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
        View ntpLayout = ntp.getLayout();
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
                ntp.getLayout().findViewById(R.id.tab_switcher_module_container);
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
        int currentTabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> cta.getCurrentTabModel().getCount());
        Assert.assertEquals(tabCount, currentTabCount);
        Tab tab = HomeSurfaceTestUtils.getCurrentTabFromUiThread(cta);
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
        assertThat(tab.isIncognito()).isFalse();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(tab.getNativePage(), Matchers.instanceOf(NewTabPage.class));
                    Criteria.checkThat(
                            ((NewTabPage) tab.getNativePage()).isLoadedForTests(),
                            Matchers.is(true));
                });
    }

    private void verifyFakeSearchBoxWidth() {
        // Orientation changes are not supported on automotive.
        if (DeviceInfo.isAutomotive()) {
            verifyFakeSearchBoxWidthForCurrentOrientation();
            return;
        }

        // Start off in landscape screen orientation.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        verifyFakeSearchBoxWidthForCurrentOrientation();

        // Switch to portrait screen orientation.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        verifyFakeSearchBoxWidthForCurrentOrientation();
    }

    private int calculateExpectedWidthSlack(int currentWidth, Resources res) {
        int contentTwoSideMarginConstant =
                2 * res.getDimensionPixelSize(R.dimen.ntp_search_box_lateral_margin_tablet);
        int maxContentWidth = res.getDimensionPixelSize(R.dimen.ntp_search_box_max_width);
        return Math.max(contentTwoSideMarginConstant, currentWidth - maxContentWidth);
    }

    private void verifyFakeSearchBoxWidthForCurrentOrientation() {
        CriteriaHelper.pollUiThread(
                () -> {
                    // Re-fetch NTP to avoid potential staleness after orientation change.
                    NewTabPage ntp =
                            (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
                    Resources res = ntp.getView().getResources();

                    View ntpLayout = ntp.getLayout();
                    View searchBoxLayout = ntpLayout.findViewById(R.id.search_box);
                    int width = ntpLayout.getWidth();

                    // Expected: Calculate the expected slack mathematically from constants.
                    int expectedSlack = calculateExpectedWidthSlack(width, res);

                    // Actual: Assert the actual search box matches the expected slack.
                    Criteria.checkThat(
                            "Fake search box width is inconsistent with screen width.",
                            width - searchBoxLayout.getWidth(),
                            Matchers.is(expectedSlack));
                });
    }

    private void verifyMostVisitedTileMargin() {
        // Orientation changes are not supported on automotive.
        if (DeviceInfo.isAutomotive()) {
            verifyTileMargin();
            return;
        }

        // Start off in landscape screen orientation.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        verifyTileMargin();

        // Switch to portrait screen orientation.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        verifyTileMargin();
    }

    private void verifyTileMargin() {
        CriteriaHelper.pollUiThread(
                () -> {
                    // Re-fetch NTP to avoid potential staleness after orientation change.
                    NewTabPage ntp =
                            (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
                    Resources res = ntp.getView().getResources();
                    int expectedEdgeMargin =
                            res.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
                    int expectedIntervalMargin =
                            res.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet);

                    View ntpLayout = ntp.getLayout();
                    int width = ntpLayout.getWidth();

                    View mvtContainer = ntpLayout.findViewById(R.id.mv_tiles_container);
                    TilesLinearLayout mvTilesLayout = ntpLayout.findViewById(R.id.mv_tiles_layout);

                    Criteria.checkThat(
                            "Not enough tiles to verify margins",
                            mvTilesLayout.getChildCount(),
                            Matchers.greaterThan(1));

                    // Expected: Calculate the expected slack mathematically from constants.
                    int expectedContainerWidthSlack = calculateExpectedWidthSlack(width, res);

                    // Actual: Assert the actual container matches the expected slack.
                    Criteria.checkThat(
                            "MVT container width is inconsistent with screen width.",
                            width - mvtContainer.getWidth(),
                            Matchers.is(expectedContainerWidthSlack));

                    int mvt1LeftMargin =
                            ((MarginLayoutParams) mvTilesLayout.getTileAt(0).getLayoutParams())
                                    .leftMargin;
                    int mvt2LeftMargin =
                            ((MarginLayoutParams) mvTilesLayout.getTileAt(1).getLayoutParams())
                                    .leftMargin;

                    Criteria.checkThat(
                            mvt1LeftMargin, Matchers.greaterThanOrEqualTo(expectedEdgeMargin));
                    Criteria.checkThat(mvt2LeftMargin, Matchers.is(expectedIntervalMargin));
                });
    }
}
