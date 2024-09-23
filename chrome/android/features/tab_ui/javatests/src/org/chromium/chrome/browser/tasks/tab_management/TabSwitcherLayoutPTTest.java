// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;
import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.CarryOn;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations.EnableAnimations;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabThumbnailsCapturedCarryOn;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;

/** Tests for the {@link TabSwitcherLayout}. */
@SuppressWarnings("ConstantConditions")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID, ChromeFeatureList.ANDROID_HUB_SEARCH})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Batch(Batch.PER_CLASS)
public class TabSwitcherLayoutPTTest {

    private static final String TEST_URL = "/chrome/test/data/android/google.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .build();

    private static EmbeddedTestServer sTestServer;

    private WebPageStation mStartPage;
    private WeakReference<Bitmap> mBitmap;

    @Before
    public void setUp() throws ExecutionException {
        // After setUp, Chrome is launched and has one NTP.
        mStartPage = mInitialStateRule.startOnBlankPage();

        sActivityTestRule
                .getActivity()
                .getTabContentManager()
                .setCaptureMinRequestTimeForTesting(0);
    }

    @BeforeClass
    public static void setUpClass() throws ExecutionException {
        sTestServer = sActivityTestRule.getTestServer();
    }

    /** Enters the regular Tab Switcher, making sure all tabs have a thumbnail. */
    private RegularTabSwitcherStation enterRegularHTSWithThumbnailChecking(
            PageStation currentStation) {
        RegularTabSwitcherStation tabSwitcherStation = currentStation.openRegularTabSwitcher();
        CarryOn.pickUp(
                new TabThumbnailsCapturedCarryOn(/* isIncognito= */ false), /* trigger= */ null);
        return tabSwitcherStation;
    }

    /** Enters the Incognito Tab Switcher, making sure all tabs have a thumbnail. */
    private IncognitoTabSwitcherStation enterIncognitoHTSWithThumbnailChecking(
            PageStation currentStation) {
        IncognitoTabSwitcherStation tabSwitcherStation = currentStation.openIncognitoTabSwitcher();
        CarryOn.pickUp(
                new TabThumbnailsCapturedCarryOn(/* isIncognito= */ true), /* trigger= */ null);
        return tabSwitcherStation;
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderGrid_10WebTabs() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        mStartPage, 10, 0, "about:blank", WebPageStation::newBuilder);
        // Make sure all thumbnails are there before switching tabs.
        RegularTabSwitcherStation tabSwitcherStation =
                enterRegularHTSWithThumbnailChecking(pageStation);
        pageStation = tabSwitcherStation.selectTabAtIndex(0, WebPageStation.newBuilder());

        tabSwitcherStation = pageStation.openRegularTabSwitcher();
        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "10_web_tabs");

        WebPageStation previousPage =
                tabSwitcherStation.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderGrid_10WebTabs_InitialScroll() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        mStartPage, 10, 0, "about:blank", WebPageStation::newBuilder);
        assertEquals(9, cta.getTabModelSelector().getCurrentModel().index());
        RegularTabSwitcherStation tabSwitcherStation =
                enterRegularHTSWithThumbnailChecking(pageStation);
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "10_web_tabs-select_last");

        WebPageStation previousPage =
                tabSwitcherStation.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Test is flaky due to thumbnails not being reliably captured")
    public void testRenderGrid_3WebTabs() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        mStartPage, 3, 0, sTestServer.getURL(TEST_URL), WebPageStation::newBuilder);
        // Make sure all thumbnails are there before switching tabs.
        RegularTabSwitcherStation tabSwitcherStation =
                enterRegularHTSWithThumbnailChecking(pageStation);
        pageStation = tabSwitcherStation.selectTabAtIndex(0, WebPageStation.newBuilder());

        tabSwitcherStation = pageStation.openRegularTabSwitcher();

        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "3_web_tabs");

        WebPageStation previousPage =
                tabSwitcherStation.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderGrid_3NativeTabs() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        RegularNewTabPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        mStartPage,
                        3,
                        0,
                        UrlConstants.NTP_URL,
                        RegularNewTabPageStation::newBuilder);
        // Make sure all thumbnails are there before switching tabs.
        RegularTabSwitcherStation tabSwitcherStation =
                enterRegularHTSWithThumbnailChecking(pageStation);
        pageStation = tabSwitcherStation.selectTabAtIndex(0, RegularNewTabPageStation.newBuilder());

        tabSwitcherStation = pageStation.openRegularTabSwitcher();

        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "3_native_tabs");

        RegularNewTabPageStation previousPage =
                tabSwitcherStation.leaveHubToPreviousTabViaBack(
                        RegularNewTabPageStation.newBuilder());
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Test is flaky due to thumbnails not being reliably captured")
    @RequiresRestart("Disable batching while re-enabling other tests.")
    public void testRenderGrid_Incognito() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        // Prepare some incognito tabs and enter tab switcher.
        WebPageStation pageStation =
                Journeys.createTabsWithThumbnails(
                        mStartPage,
                        3,
                        "about:blank",
                        /* isIncognito= */ true,
                        WebPageStation::newBuilder);
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        IncognitoTabSwitcherStation tabSwitcherStation =
                enterIncognitoHTSWithThumbnailChecking(pageStation);
        pageStation = tabSwitcherStation.selectTabAtIndex(0, WebPageStation.newBuilder());
        tabSwitcherStation = pageStation.openIncognitoTabSwitcher();
        ChromeRenderTestRule.sanitize(cta.findViewById(R.id.pane_frame));
        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "3_incognito_web_tabs");

        WebPageStation previousPage =
                tabSwitcherStation.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    @DisabledTest(message = "Test is flaky due to thumbnails not being reliably captured")
    public void testRenderGrid_1TabGroup_ColorIcon() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();

        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularNewTabPageStation secondPage = firstPage.openRegularTabAppMenu().openNewTab();
        int secondTabId = secondPage.getLoadedTab().getId();
        // Make sure all thumbnails are there before switching tabs.
        RegularTabSwitcherStation tabSwitcher = enterRegularHTSWithThumbnailChecking(secondPage);
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        NewTabGroupDialogFacility dialog =
                editor.openAppMenuWithEditor().groupTabsWithParityEnabled();
        dialog = dialog.inputName("test_tab_group_name");
        dialog = dialog.pickColor(TabGroupColorId.RED);
        dialog.pressDone();

        ChromeRenderTestRule.sanitize(cta.findViewById(R.id.pane_frame));
        mRenderTestRule.render(
                cta.findViewById(R.id.pane_frame), "1_tab_group_GTS_card_item_color_icon");

        WebPageStation previousPage =
                tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    @EnableAnimations
    public void testTabToGridAndBack_NoReset() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        WebPageStation page =
                roundtripToHTSWithThumbnailChecks(
                        firstPage,
                        WebPageStation::newBuilder,
                        () -> {},
                        /* canGarbageCollectBitmaps= */ false);
        assertFinalDestination(page);
    }

    @Test
    @MediumTest
    @EnableAnimations
    public void testTabToGridAndBack_SoftCleanup() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        Runnable resetHTSStateOnUiThread =
                () -> {
                    var tabSwitcherPane =
                            (TabSwitcherPaneBase)
                                    cta.getHubManagerSupplierForTesting()
                                            .get()
                                            .getPaneManager()
                                            .getFocusedPaneSupplier()
                                            .get();
                    tabSwitcherPane.softCleanupForTesting();
                };
        WebPageStation page =
                roundtripToHTSWithThumbnailChecks(
                        firstPage,
                        WebPageStation::newBuilder,
                        resetHTSStateOnUiThread,
                        /* canGarbageCollectBitmaps= */ true);
        assertFinalDestination(page);
    }

    @Test
    @MediumTest
    @EnableAnimations
    public void testTabToGridAndBack_SoftCleanup_Ntp() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        RegularNewTabPageStation ntp = firstPage.openRegularTabAppMenu().openNewTab();
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        Runnable resetHTSStateOnUiThread =
                () -> {
                    var tabSwitcherPane =
                            (TabSwitcherPaneBase)
                                    cta.getHubManagerSupplierForTesting()
                                            .get()
                                            .getPaneManager()
                                            .getFocusedPaneSupplier()
                                            .get();
                    tabSwitcherPane.softCleanupForTesting();
                };
        ntp =
                roundtripToHTSWithThumbnailChecks(
                        ntp,
                        RegularNewTabPageStation::newBuilder,
                        resetHTSStateOnUiThread,
                        /* canGarbageCollectBitmaps= */ true);
        assertFinalDestination(ntp);
    }

    @Test
    @MediumTest
    @EnableAnimations
    public void testTabToGridAndBack_HardCleanup() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        Runnable resetHTSStateOnUiThread =
                () -> {
                    var tabSwitcherPane =
                            (TabSwitcherPaneBase)
                                    cta.getHubManagerSupplierForTesting()
                                            .get()
                                            .getPaneManager()
                                            .getFocusedPaneSupplier()
                                            .get();
                    tabSwitcherPane.softCleanupForTesting();
                    tabSwitcherPane.hardCleanupForTesting();
                };
        WebPageStation page =
                roundtripToHTSWithThumbnailChecks(
                        firstPage,
                        WebPageStation::newBuilder,
                        resetHTSStateOnUiThread,
                        /* canGarbageCollectBitmaps= */ true);
        assertFinalDestination(page);
    }

    @Test
    @MediumTest
    @EnableAnimations
    public void testTabToGridAndBack_NoCoordinator() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        Runnable resetHTSStateOnUiThread =
                () -> {
                    var tabSwitcherPane =
                            (TabSwitcherPaneBase)
                                    cta.getHubManagerSupplierForTesting()
                                            .get()
                                            .getPaneManager()
                                            .getFocusedPaneSupplier()
                                            .get();
                    tabSwitcherPane.softCleanupForTesting();
                    tabSwitcherPane.hardCleanupForTesting();
                    tabSwitcherPane.destroyCoordinatorForTesting();
                };
        WebPageStation page =
                roundtripToHTSWithThumbnailChecks(
                        firstPage,
                        WebPageStation::newBuilder,
                        resetHTSStateOnUiThread,
                        /* canGarbageCollectBitmaps= */ true);
        assertFinalDestination(page);
    }

    private <T extends PageStation> T roundtripToHTSWithThumbnailChecks(
            T page,
            Supplier<PageStation.Builder<T>> destinationBuiderFactory,
            Runnable resetHTSStateOnUiThread,
            boolean canGarbageCollectBitmaps) {
        RegularTabSwitcherStation tabSwitcher = enterRegularHTSWithThumbnailChecking(page);

        // TODO(crbug.com/324919909): Migrate this to a HubTabSwitcherCardFacility with a tab
        // thumbnail as a view element.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ImageView view =
                            (ImageView)
                                    sActivityTestRule
                                            .getActivity()
                                            .findViewById(R.id.tab_thumbnail);
                    mBitmap =
                            new WeakReference<>(((BitmapDrawable) view.getDrawable()).getBitmap());
                    assertNotNull(mBitmap.get());
                });

        page = tabSwitcher.leaveHubToPreviousTabViaBack(destinationBuiderFactory.get());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    resetHTSStateOnUiThread.run();
                });

        if (canGarbageCollectBitmaps) {
            assertTrue(canBeGarbageCollected(mBitmap));
        } else {
            assertFalse(canBeGarbageCollected(mBitmap));
        }

        tabSwitcher = enterRegularHTSWithThumbnailChecking(page);
        return tabSwitcher.leaveHubToPreviousTabViaBack(destinationBuiderFactory.get());
    }
}
