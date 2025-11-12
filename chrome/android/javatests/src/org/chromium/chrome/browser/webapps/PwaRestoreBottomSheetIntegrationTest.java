// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isNotEnabled;

import static org.chromium.base.test.transit.Triggers.noopTo;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.PwaRestoreBottomSheetTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.webapps.PwaRestorePromoUtils.DisplayStage;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.webapps.PwaRestoreCarryOn;
import org.chromium.chrome.test.transit.webapps.PwaRestoreHiddenCarryOn;
import org.chromium.chrome.test.transit.webapps.PwaReviewAppEntryCarryOn;
import org.chromium.chrome.test.transit.webapps.PwaReviewCarryOn;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Test the showing of the PWA Restore Bottom Sheet dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Function getURL for EmbeddedTestServer fails when batching")
@EnableFeatures({
    ChromeFeatureList.PWA_RESTORE_UI,
    ChromeFeatureList.WEB_APK_BACKUP_AND_RESTORE_BACKEND
})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PwaRestoreBottomSheetIntegrationTest {
    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final @DisplayStage int sFlagValueMissing = DisplayStage.UNKNOWN_STATUS;

    private static final String ICON_URL1 = "/chrome/test/data/banners/256x256-green.png";
    private static final String ICON_URL2 = "/chrome/test/data/banners/256x256-red.png";

    private static final String[][] sDefaultApps = {
        {"https://example.com/app1/", "App 1", ICON_URL1},
        {"https://example.com/app2/", "App 2", ICON_URL2},
        {"https://example.com/app3/", "App 3", ICON_URL1}
    };
    private static final String TAG = "PwaRestoreIntegrTest";

    private SharedPreferencesManager mPreferences;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mPreferences = ChromeSharedPreferences.getInstance();
        mTestServer = mActivityTestRule.getTestServer();

        // Promos only run *after* the first run experience has completed, so we need to make sure
        // the testing environment reflects that. Note that individual tests below can set whether
        // the first run experience completed just now or in a previous launch.
        FirstRunStatus.setFirstRunFlowComplete(true);
        // The first run sequence always suppresses all promos during the first launch, so we must
        // do the same to make sure the testing environment reflects what happens during normal
        // startup.
        mPreferences.writeBoolean(ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, true);
    }

    private void setTestAppsForRestoring(String[][] appList) {
        for (String[] app : appList) {
            app[2] = mTestServer.getURL(app[2]);
        }

        try {
            PwaRestoreBottomSheetTestUtils.waitForWebApkDatabaseInitialization();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for WebApk database initialization", e);
        }
        PwaRestoreBottomSheetTestUtils.setAppListForRestoring(appList);
    }

    @After
    public void tearDown() {
        // Clean up the pref we created.
        mPreferences.removeKeySync(ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE);
        mPreferences.removeKeySync(ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    public void testInitialLaunchOnNewProfile() {
        // This test simulates the very first launch of Chrome on a new device. The test makes sure
        // that once the first run experience is triggered, the PwaRestore promo gets notified about
        // it, but the promo is not shown.

        // To do this, we need to set the FirstRunStatus to simulate that the first run
        // experience has been triggered during this launch.
        FirstRunStatus.setFirstRunTriggeredForTesting(true);

        // At the beginning, there should be no signal, but at the end we should be ready to show
        // the promo during the next launch (see `testSecondLaunchAfterBeingNotified`).
        assertCurrentFlag(sFlagValueMissing);
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreHiddenCarryOn());
        assertCurrentFlag(DisplayStage.SHOW_PROMO);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    public void testSecondLaunchAfterBeingNotified() {
        // This test makes sure that on the subsequent launch -- after getting notified about the
        // first run experience having triggered already -- the promo dialog is showing and the
        // right pref has been written (`ALREADY_LAUNCHED`) to make sure we don't show again.
        setAppsAvailableAndPromoStage(true, DisplayStage.SHOW_PROMO);

        assertCurrentFlag(DisplayStage.SHOW_PROMO);
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreCarryOn());
        assertCurrentFlag(DisplayStage.ALREADY_LAUNCHED);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    public void testSecondLaunchAfterBeingNotifiedButNoApps() {
        // This test makes sure that if we get notified to show, but no apps are available to
        // restore,
        // that we don't show the promo and suppress it from showing in the future.
        setAppsAvailableAndPromoStage(false, DisplayStage.SHOW_PROMO);

        assertCurrentFlag(DisplayStage.SHOW_PROMO);
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreHiddenCarryOn());
        assertCurrentFlag(DisplayStage.NO_APPS_AVAILABLE);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    public void testEveryLaunchAfterShowing() {
        // This test makes sure that after showing the dialog once, the flag remains set and the
        // dialog is not shown again.
        setAppsAvailableAndPromoStage(true, DisplayStage.ALREADY_LAUNCHED);

        assertCurrentFlag(DisplayStage.ALREADY_LAUNCHED);
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreHiddenCarryOn());
        assertCurrentFlag(DisplayStage.ALREADY_LAUNCHED);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    public void testInitialLaunchOnPreexistingProfile() {
        // This test makes sure that, if the first run experience was completed in the past
        // (simulated by not calling `setFirstRunTriggeredForTesting(true)`), that it is treated
        // as if the profile is pre-existing, and we don't show the promo (ever).

        mPreferences.writeBoolean(ChromePreferenceKeys.PWA_RESTORE_APPS_AVAILABLE, true);

        assertCurrentFlag(sFlagValueMissing);
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreHiddenCarryOn());
        assertCurrentFlag(DisplayStage.PRE_EXISTING_PROFILE);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    public void testEveryLaunchAfterDetectingNoShow() {
        // This test makes sure that after determining the profile is too old to show the promo, the
        // flag remains set and the dialog is not shown.
        setAppsAvailableAndPromoStage(true, DisplayStage.PRE_EXISTING_PROFILE);

        assertCurrentFlag(DisplayStage.PRE_EXISTING_PROFILE);
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreHiddenCarryOn());
        assertCurrentFlag(DisplayStage.PRE_EXISTING_PROFILE);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    @DisabledTest(message = "https://crbug.com/425736622")
    public void testBackButton() {
        // This test opens the dialog, clicks the Review button to expand the bottom sheet dialog
        // and then presses the Back in the OS twice to see what happens (first click should
        // navigate back to the initial dialog state, second click closes the dialog).

        setTestAppsForRestoring(sDefaultApps);

        // Ensure the promo dialog shows.
        setAppsAvailableAndPromoStage(true, DisplayStage.SHOW_PROMO);

        // Start from launched and verify we're in initial state for the dialog.
        PwaRestoreCarryOn pwaRestore =
                mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreCarryOn());

        // Go to PWA list mode.
        PwaReviewCarryOn pwaReview = pwaRestore.clickReview();

        // Pressing the Back button in Android once should bring us to the initial dialog state.
        pwaRestore = pwaReview.pressBackToReturn();

        // Pressing the Back button again should close the bottom sheet.
        pwaRestore.pressBackTo().dropCarryOn();
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    @DisabledTest(message = "https://crbug.com/425736622")
    public void testClickForwarding() {
        setTestAppsForRestoring(sDefaultApps);

        // Ensure the promo dialog shows.
        setAppsAvailableAndPromoStage(true, DisplayStage.SHOW_PROMO);

        PwaReviewCarryOn pwaReview =
                mActivityTestRule
                        .startFromLauncherTo()
                        .pickUpCarryOn(new PwaRestoreCarryOn())
                        .clickReview();

        PwaReviewAppEntryCarryOn appEntry = pwaReview.focusOnEntry("App 1");
        noopTo().waitFor(appEntry.isSelected());

        appEntry.appNameElement.clickTo().waitFor(appEntry.isUnselected());
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    public void testButtonsInitiallyDisabled() throws Exception {
        // Ensure the promo dialog shows.
        setAppsAvailableAndPromoStage(true, DisplayStage.SHOW_PROMO);

        PwaRestoreCarryOn pwaRestore =
                mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreCarryOn());

        // Ensure Deselect and Restore buttons are disabled (nothing to act on).
        PwaReviewCarryOn pwaReview = pwaRestore.clickReview();
        noopTo().waitFor(
                        pwaReview.deselectButtonElement.matches(isNotEnabled()),
                        pwaReview.restoreButtonElement.matches(isNotEnabled()));
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    @DisabledTest(message = "https://crbug.com/425736622")
    public void testDeselectAll() {
        setTestAppsForRestoring(sDefaultApps);

        // Ensure the promo dialog shows.
        setAppsAvailableAndPromoStage(true, DisplayStage.SHOW_PROMO);

        PwaRestoreCarryOn pwaRestore =
                mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreCarryOn());

        // Deselect and Restore buttons should start in enabled state.
        PwaReviewCarryOn pwaReview = pwaRestore.clickReview();
        pwaReview.deselectButtonElement.check(matches(isEnabled()));
        pwaReview.restoreButtonElement.check(matches(isEnabled()));

        var app1Entry = pwaReview.focusOnEntry("App 1");
        var app2Entry = pwaReview.focusOnEntry("App 2");
        var app3Entry = pwaReview.focusOnEntry("App 3");
        noopTo().waitFor(app1Entry.isSelected(), app2Entry.isSelected(), app3Entry.isSelected());

        // Now verify the Deselect function leaves everything in unchecked state.
        // Deselect and Restore buttons should now be disabled (nothing to act on).
        pwaReview
                .deselectButtonElement
                .clickTo()
                .waitFor(
                        app1Entry.isUnselected(),
                        app2Entry.isUnselected(),
                        app3Entry.isUnselected(),
                        pwaReview.deselectButtonElement.matches(isNotEnabled()),
                        pwaReview.restoreButtonElement.matches(isNotEnabled()));

        // Ensure one entry gets checked.
        // Deselect and Restore buttons become enabled since we have something to act on.
        app1Entry
                .appNameElement
                .clickTo()
                .waitFor(
                        app1Entry.isSelected(),
                        app2Entry.isUnselected(),
                        app3Entry.isUnselected(),
                        pwaReview.deselectButtonElement.matches(isEnabled()),
                        pwaReview.restoreButtonElement.matches(isEnabled()));

        // Ensure same entry gets unchecked again clicking the checkbox itself.
        // Deselect and Restore buttons become disabled since no item remains selected.
        app1Entry
                .checkboxElement
                .clickTo()
                .waitFor(
                        app1Entry.isUnselected(),
                        app2Entry.isUnselected(),
                        app3Entry.isUnselected(),
                        pwaReview.deselectButtonElement.matches(isNotEnabled()),
                        pwaReview.restoreButtonElement.matches(isNotEnabled()));
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    @DisabledTest(message = "https://crbug.com/425736622")
    public void testRestoreClosesUi() {
        setTestAppsForRestoring(sDefaultApps);

        // Ensure the promo dialog shows.
        setAppsAvailableAndPromoStage(true, DisplayStage.SHOW_PROMO);

        PwaReviewCarryOn pwaReview =
                mActivityTestRule
                        .startFromLauncherTo()
                        .pickUpCarryOn(new PwaRestoreCarryOn())
                        .clickReview();

        pwaReview
                .restoreButtonElement
                .clickTo()
                .dropCarryOnAnd()
                .pickUpCarryOn(new PwaRestoreHiddenCarryOn());
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    @DisableFeatures({ChromeFeatureList.PWA_RESTORE_UI_AT_STARTUP})
    public void testForceFlagOff() {
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreHiddenCarryOn());
    }

    @Test
    @SmallTest
    @Feature({"PwaRestore"})
    @EnableFeatures({ChromeFeatureList.PWA_RESTORE_UI_AT_STARTUP})
    public void testForceFlagOn() {
        mActivityTestRule.startFromLauncherTo().pickUpCarryOn(new PwaRestoreCarryOn());
    }

    private void setAppsAvailableAndPromoStage(boolean appsAvailable, @DisplayStage int value) {
        mPreferences.writeInt(ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, value);
        mPreferences.writeBoolean(ChromePreferenceKeys.PWA_RESTORE_APPS_AVAILABLE, appsAvailable);
    }

    private void assertCurrentFlag(@DisplayStage int value) {
        Assert.assertEquals(
                value,
                mPreferences.readInt(
                        ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.UNKNOWN_STATUS));
    }
}
