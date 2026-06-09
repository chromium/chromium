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

import android.content.pm.ActivityInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
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
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisableIf;
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
import org.chromium.chrome.browser.logo.LegacyLogoView;
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
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/** Integration tests of showing a NTP with Start surface UI at startup. */
@RunWith(ChromeJUnit4ClassRunner.class)
// Restrict to Phones and Tablets because Desktop Android does not show NTP at startup.
@Restriction(DeviceFormFactor.PHONE_OR_TABLET)
@EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME})
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(reason = "This test suite tests startup behaviors.")
public class ShowNtpAtStartupTest {
    private static final int RENDER_TEST_REVISION = 1;

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
    @EnableFeatures({ChromeFeatureList.LOGO_VIEW_REFACTOR})
    public void testNtpLogoSize_logoViewRefactorFlagEnabled() {
        mActivityTestRule.startOnNtp();
        Resources res = mActivityTestRule.getActivity().getResources();
        int expectedLogoHeight = res.getDimensionPixelSize(R.dimen.ntp_logo_height);
        int expectedTopMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int expectedBottomMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);

        // Verifies the logo size is decreased, and top bottom margins are updated.
        testLogoSizeImpl_logoViewRefactorEnabled(
                expectedLogoHeight, expectedTopMargin, expectedBottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableFeatures({ChromeFeatureList.LOGO_VIEW_REFACTOR})
    public void testNtpLogoSize_logoViewRefactorFlagDisabled() {
        mActivityTestRule.startOnNtp();
        Resources res = mActivityTestRule.getActivity().getResources();
        int expectedLogoHeight = res.getDimensionPixelSize(R.dimen.ntp_logo_height);
        int expectedTopMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int expectedBottomMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);

        // Verifies the logo size is decreased, and top bottom margins are updated.
        testLogoSizeImpl_logoViewRefactorDisabled(
                expectedLogoHeight, expectedTopMargin, expectedBottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures({ChromeFeatureList.LOGO_VIEW_REFACTOR})
    public void testNtpDoodleSize_logoViewRefactorFlagEnabled() {
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
                            new Logo(Bitmap.createBitmap(1, 1, Config.ALPHA_8), null, null, null);
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
        testLogoSizeImpl_logoViewRefactorEnabled(
                expectedLogoHeight, expectedTopMargin, expectedBottomMargin);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @DisableFeatures({ChromeFeatureList.LOGO_VIEW_REFACTOR})
    public void testNtpDoodleSize_logoViewRefactorFlagDisabled() {
        mActivityTestRule.startOnNtp();

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        final int[] expectedValues = new int[3];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LegacyLogoView logoView =
                            (LegacyLogoView) ntp.getView().findViewById(R.id.search_provider_logo);
                    Logo logo =
                            new Logo(Bitmap.createBitmap(1, 1, Config.ALPHA_8), null, null, null);
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
        testLogoSizeImpl_logoViewRefactorDisabled(
                expectedLogoHeight, expectedTopMargin, expectedBottomMargin);
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

    private void testLogoSizeImpl_logoViewRefactorEnabled(
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

    private void testLogoSizeImpl_logoViewRefactorDisabled(
            int expectedLogoHeight, int expectedTopMargin, int expectedBottomMargin) {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        View logoView = ntp.getView().findViewById(R.id.search_provider_logo);

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
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
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
    @Feature({"StartSurface", "RenderTest"})
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures({START_SURFACE_RETURN_TIME_IMMEDIATE})
    public void testFakeSearchBoxWidth_phones() throws IOException {
        mActivityTestRule.startFromLauncherAtNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);

        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        View ntpLayout = ntp.getLayout();
        View searchBoxLayout = ntpLayout.findViewById(R.id.search_box);

        // Orientation changes are not supported on automotive.
        if (DeviceInfo.isAutomotive()) {
            mRenderTestRule.render(searchBoxLayout, "ntp_search_box_automotive");
            return;
        }

        // Start off in landscape screen orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForScreenOrientation("\"landscape\"");
        mRenderTestRule.render(searchBoxLayout, "ntp_search_box_landscape");

        // Start off in portrait screen orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForScreenOrientation("\"portrait\"");
        // Verifies there is additional margins added for the fake search box.
        mRenderTestRule.render(searchBoxLayout, "ntp_search_box_portrait");
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/442027285
    public void testFakeSearchBoxWidth() {
        mActivityTestRule.startOnNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();

        Resources res = cta.getResources();
        int expectedTwoSideMargin =
                2 * res.getDimensionPixelSize(R.dimen.ntp_search_box_lateral_margin_tablet);

        // Verifies there is additional margin added for the fake search box.
        verifyFakeSearchBoxWidth(expectedTwoSideMargin, expectedTwoSideMargin, ntp);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/442027285
    @EnableFeatures(START_SURFACE_RETURN_TIME_IMMEDIATE)
    @DisabledTest(message = "https://crbug.com/442027285")
    public void testMvtLayoutHorizontalMargin() {
        mActivityTestRule.startOnNtp();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        HomeSurfaceTestUtils.waitForTabModel(cta);
        waitForNtpLoaded(mActivityTestRule.getActivityTab());

        NewTabPage ntp = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();

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

    private void verifyFakeSearchBoxWidth(
            int expectedLandScapeWidth, int expectedPortraitWidth, NewTabPage ntp) {
        View ntpLayout = ntp.getLayout();
        View searchBoxLayout = ntpLayout.findViewById(R.id.search_box);

        // Orientation changes are not supported on automotive.
        if (DeviceInfo.isAutomotive()) {
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
            View ntpLayout,
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
        View ntpLayout = ntp.getLayout();
        View mvtContainer = ntpLayout.findViewById(R.id.mv_tiles_container);
        TilesLinearLayout mvTilesLayout = ntpLayout.findViewById(R.id.mv_tiles_layout);
        int mvt1LeftMargin =
                ((MarginLayoutParams) mvTilesLayout.getTileAt(0).getLayoutParams()).leftMargin;
        int mvt2LeftMargin =
                ((MarginLayoutParams) mvTilesLayout.getTileAt(1).getLayoutParams()).leftMargin;

        // Orientation changes are not supported on automotive.
        if (DeviceInfo.isAutomotive()) {
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
            View ntpLayout,
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
