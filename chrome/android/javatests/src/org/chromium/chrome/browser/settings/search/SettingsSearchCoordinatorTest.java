// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.atomic.AtomicReference;

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

    private @Nullable SettingsActivity getSettingsActivity() {
        for (Activity a :
                ActivityLifecycleMonitorRegistry.getInstance()
                        .getActivitiesInStage(Stage.RESUMED)) {
            if (a instanceof SettingsActivity) {
                SettingsActivity settingsActivity = (SettingsActivity) a;
                if (!settingsActivity.isDestroyed() && !settingsActivity.isFinishing()) {
                    return settingsActivity;
                }
            }
        }
        return null;
    }

    private @Nullable SettingsSearchCoordinator getSearchCoordinator() {
        SettingsActivity settingsActivity = getSettingsActivity();
        if (settingsActivity == null) return null;
        return settingsActivity.getSearchCoordinatorForTesting();
    }

    private SettingsActivity waitForSettingsActivity() {
        final AtomicReference<SettingsActivity> activityRef = new AtomicReference<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    SettingsActivity activity = getSettingsActivity();
                    if (activity == null) return false;
                    activityRef.set(activity);
                    return true;
                });
        return activityRef.get();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testBasicSearch() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();

        CallbackHelper callbackHelper = new CallbackHelper();

        CriteriaHelper.pollUiThread(
                () -> {
                    SettingsSearchCoordinator searchCoordinator = getSearchCoordinator();
                    if (searchCoordinator == null) return false;

                    try {
                        searchCoordinator.enterSearchState(/* isRestored= */ false);
                        searchCoordinator.performSearch(
                                "a",
                                (results) -> {
                                    assertFalse(results.getItems().isEmpty());
                                    callbackHelper.notifyCalled();
                                });
                        return true;
                    } catch (IllegalStateException e) {
                        return false; // FragmentManager destroyed during enterSearchState
                    }
                });

        callbackHelper.waitForCallback(0);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testRecentSearchIsRestored() throws Throwable {
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity activity = waitForSettingsActivity();
        SettingsSearchCoordinator searchCoordinator = activity.getSearchCoordinatorForTesting();
        assertFalse(searchCoordinator.hasRecentSearchEntriesForTesting());

        var resultDisplayHelper = new CallbackHelper();
        int callCount = resultDisplayHelper.getCallCount();

        // Search for 'Privacy Guide'.
        int titleId = R.string.privacy_guide_pref_title;
        String query = activity.getString(titleId);
        CriteriaHelper.pollUiThread(
                () -> {
                    SettingsSearchCoordinator currentCoordinator = getSearchCoordinator();
                    if (currentCoordinator == null) return false;

                    try {
                        currentCoordinator.enterSearchState(/* isRestored= */ false);
                        currentCoordinator.performSearch(
                                query,
                                (results) -> {
                                    currentCoordinator.displayResultsFragment(results);
                                    resultDisplayHelper.notifyCalled();
                                });
                        return true;
                    } catch (IllegalStateException e) {
                        return false; // FragmentManager destroyed during enterSearchState
                    }
                });

        resultDisplayHelper.waitForCallback(callCount);
        onView(withText(titleId)).perform(click());
        assertTrue(searchCoordinator.hasRecentSearchEntriesForTesting());

        activity.finish();
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);

        // Verify that recent search is restored from disk after restarting the settings.
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity activity2 = waitForSettingsActivity();
        SettingsSearchCoordinator searchCoordinator2 = activity2.getSearchCoordinatorForTesting();
        assertTrue(searchCoordinator2.hasRecentSearchEntriesForTesting());
    }
}
