// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for the Search in Settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
@EnableFeatures(ChromeFeatureList.SEARCH_IN_SETTINGS)
public class SettingsSearchCoordinatorTest {
    @Rule
    public SettingsActivityTestRule<?> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(null);

    @After
    public void tearDown() {
        mSettingsActivityTestRule.getActivity().finish();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testBasicSearch() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        SettingsSearchCoordinator searchCoordinator = activity.getSearchCoordinatorForTesting();
        assertNotNull(searchCoordinator);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    searchCoordinator.enterSearchState(/* isRestored= */ false);
                    searchCoordinator.performSearch(
                            "a", (results) -> assertFalse(results.getItems().isEmpty()));
                });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testRecentSearchIsRestored() throws Throwable {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        SettingsSearchCoordinator searchCoordinator = activity.getSearchCoordinatorForTesting();
        assertFalse(searchCoordinator.hasRecentSearchEntriesForTesting());

        var resultDisplayHelper = new CallbackHelper();
        int callCount = resultDisplayHelper.getCallCount();

        // Search for 'Privacy Guide'.
        int titleId = R.string.privacy_guide_pref_title;
        String query = activity.getString(titleId);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    searchCoordinator.enterSearchState(/* isRestored= */ false);
                    searchCoordinator.performSearch(
                            query,
                            (results) -> {
                                searchCoordinator.displayResultsFragment(results);
                                resultDisplayHelper.notifyCalled();
                            });
                });
        resultDisplayHelper.waitForCallback(callCount);
        onView(withText(titleId)).perform(click());
        assertTrue(searchCoordinator.hasRecentSearchEntriesForTesting());

        activity.finish();
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);

        // Verify that recent search is restored from disk after restarting the settings.
        SettingsActivity activity2 = mSettingsActivityTestRule.startSettingsActivity();
        SettingsSearchCoordinator searchCoordinator2 = activity2.getSearchCoordinatorForTesting();
        assertTrue(searchCoordinator2.hasRecentSearchEntriesForTesting());
    }
}
