// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.ViewGroup;
import android.widget.CheckBox;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.webapps.PwaRestorePromoUtils.DisplayStage;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.webapps.R;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** Test the showing of the PWA Restore Bottom Sheet dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.PWA_RESTORE_UI})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PwaRestoreBottomSheetIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();

    private static @DisplayStage int sFlagValueMissing = DisplayStage.UNKNOWN_STATUS;

    private SharedPreferencesManager mPreferences;

    @Before
    public void setUp() {
        mPreferences = ChromeSharedPreferences.getInstance();

        // Promos only run *after* the first run experience has completed, so we need to make sure
        // the testing environment reflects that. Note that individual tests below can set whether
        // the first run experience completed just now or in a previous launch.
        FirstRunStatus.setFirstRunFlowComplete(true);
        // The first run sequence always suppresses all promos during the first launch, so we must
        // do the same to make sure the testing environment reflects what happens during normal
        // startup.
        mPreferences.writeBoolean(ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START, true);
    }

    @After
    public void tearDown() {
        // Clean up the pref we created.
        mPreferences.removeKeySync(ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE);
        mPreferences.removeKeySync(ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestrore"})
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
        mActivityTestRule.startMainActivityFromLauncher();
        assertDialogShown(false);
        assertCurrentFlag(DisplayStage.SHOW_PROMO);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestrore"})
    public void testSecondLaunchAfterBeingNotified() {
        // This test makes sure that on the subsequent launch -- after getting notified about the
        // first run experience having triggered already -- the promo dialog is showing and the
        // right pref has been written (`ALREADY_LAUNCHED`) to make sure we don't show again.
        mPreferences.writeInt(
                ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.SHOW_PROMO);

        assertCurrentFlag(DisplayStage.SHOW_PROMO);
        mActivityTestRule.startMainActivityFromLauncher();
        assertDialogShown(true);
        assertCurrentFlag(DisplayStage.ALREADY_LAUNCHED);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestrore"})
    public void testEveryLaunchAfterShowing() {
        // This test makes sure that after showing the dialog once, the flag remains set and the
        // dialog is not shown again.
        mPreferences.writeInt(
                ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.ALREADY_LAUNCHED);

        assertCurrentFlag(DisplayStage.ALREADY_LAUNCHED);
        mActivityTestRule.startMainActivityFromLauncher();
        assertDialogShown(false);
        assertCurrentFlag(DisplayStage.ALREADY_LAUNCHED);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestrore"})
    public void testInitialLaunchOnPreexistingProfile() {
        // This test makes sure that, if the first run experience was completed in the past
        // (simulated by not calling `setFirstRunTriggeredForTesting(true)`), that it is treated
        // as if the profile is pre-existing, and we don't show the promo (ever).

        assertCurrentFlag(sFlagValueMissing);
        mActivityTestRule.startMainActivityFromLauncher();
        assertDialogShown(false);
        assertCurrentFlag(DisplayStage.PRE_EXISTING_PROFILE);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestrore"})
    public void testEveryLaunchAfterDetectingNoShow() {
        // This test makes sure that after determining the profile is too old to show the promo, the
        // flag remains set and the dialog is not shown.
        mPreferences.writeInt(
                ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.PRE_EXISTING_PROFILE);

        assertCurrentFlag(DisplayStage.PRE_EXISTING_PROFILE);
        mActivityTestRule.startMainActivityFromLauncher();
        assertDialogShown(false);
        assertCurrentFlag(DisplayStage.PRE_EXISTING_PROFILE);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestrore"})
    public void testClickForwarding() {
        // Ensure the promo dialog shows.
        mPreferences.writeInt(
                ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.SHOW_PROMO);

        mActivityTestRule.startMainActivityFromLauncher();
        assertDialogShown(true);
        onView(withId(R.id.review_button)).perform(click());

        assertIsComboCheckedAtIndex(1, false);
        onView(withText("Foo")).check(matches(isDisplayed()));
        onView(withText("Foo")).perform(click());
        assertIsComboCheckedAtIndex(1, true);
    }

    @Test
    @SmallTest
    @Feature({"PwaRestrore"})
    public void testDeselectAll() {
        // Ensure the promo dialog shows.
        mPreferences.writeInt(
                ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.SHOW_PROMO);

        mActivityTestRule.startMainActivityFromLauncher();
        assertDialogShown(true);
        onView(withId(R.id.review_button)).perform(click());

        assertIsComboCheckedAtIndex(0, false);
        assertIsComboCheckedAtIndex(1, false);
        assertIsComboCheckedAtIndex(2, false);

        // Ensure one entry is checked.
        onView(withText("Foo")).check(matches(isDisplayed()));
        onView(withText("Foo")).perform(click());
        assertIsComboCheckedAtIndex(1, true);

        // Now verify the Deselect function leaves everything in unchecked state.
        onView(withId(R.id.deselect_button)).check(matches(isDisplayed()));
        onView(withId(R.id.deselect_button)).perform(click());
        assertIsComboCheckedAtIndex(0, false);
        assertIsComboCheckedAtIndex(1, false);
        assertIsComboCheckedAtIndex(2, false);
    }

    // A helper function to check whether a particular combo box in the PWA list ScrollView is
    // checked.
    private void assertIsComboCheckedAtIndex(int index, boolean checked) {
        onView(withId(R.id.scroll_view_content))
                .check(
                        (view, e) -> {
                            ViewGroup appList = (ViewGroup) view;
                            Assert.assertTrue(appList != null);

                            // The app list contains mostly appviews, but also generic views that
                            // are separators, for example. For our purposes, we want to skip the
                            // views that are not checkboxes.
                            int checkboxCount = 0;
                            for (int i = 0; i < appList.getChildCount(); ++i) {
                                CheckBox checkBox =
                                        appList.getChildAt(i).findViewById(R.id.checkbox);
                                if (checkBox != null) {
                                    if (index == checkboxCount) {
                                        Assert.assertEquals(checked, checkBox.isChecked());
                                        return;
                                    }
                                    checkboxCount += 1;
                                }
                            }
                        });
    }

    private void assertCurrentFlag(@DisplayStage int value) {
        Assert.assertEquals(
                value,
                mPreferences.readInt(
                        ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.UNKNOWN_STATUS));
    }

    private void assertDialogShown(boolean expectShowing) {
        if (expectShowing) {
            onView(withText("Restore your web apps")).check(matches(isDisplayed()));
        } else {
            onView(withText("Restore your web apps")).check(doesNotExist());
        }
    }
}
