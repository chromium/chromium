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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
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

    private void performSearch(String query) throws Exception {
        var resultDisplayHelper = new CallbackHelper();
        int callCount = resultDisplayHelper.getCallCount();
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        SettingsSearchCoordinator searchCoordinator = getSearchCoordinator();

                        searchCoordinator.enterSearchState(/* isRestored= */ false);
                        searchCoordinator.performSearch(
                                query,
                                (results) -> {
                                    searchCoordinator.displayResultsFragment(results);
                                    resultDisplayHelper.notifyCalled();
                                });
                        return true;
                    } catch (IllegalStateException e) {
                        return false; // FragmentManager destroyed during enterSearchState
                    }
                });

        resultDisplayHelper.waitForCallback(callCount);
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

        // Search for 'Privacy Guide'.
        int titleId = R.string.privacy_guide_pref_title;
        String query = activity.getString(titleId);
        performSearch(query);
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

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testHistograms_clickedResult() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity activity = waitForSettingsActivity();
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newSingleRecordWatcher(
                                    "Settings.Search.ExitReason",
                                    SettingsSearchCoordinator.ExitReason.CLICKED_RESULT);
                        });

        // Search for 'Privacy Guide' and click the result -> emit "clicked-result"
        int titleId = R.string.privacy_guide_pref_title;
        String query = activity.getString(titleId);
        performSearch(query);
        onView(withText(titleId)).perform(click());
        ThreadUtils.runOnUiThreadBlocking(() -> histograms.assertExpected());

        // Invoke OS back to get back to search UI, and perform a new search. But exit search
        // without cliking the results -> emit "abandoned-results"
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SettingsSearchCoordinator searchCoordinator = getSearchCoordinator();
                    searchCoordinator.handleBackAction();
                });
        performSearch("address");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var histograms2 =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "Settings.Search.ExitReason",
                                    SettingsSearchCoordinator.ExitReason.ABANDONED_RESULTS);
                    getSearchCoordinator().exitSearchState(/* clearFragment= */ true);
                    histograms2.assertExpected();
                });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testHistograms_abandonedResults() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity activity = waitForSettingsActivity();

        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newSingleRecordWatcher(
                                    "Settings.Search.ExitReason",
                                    SettingsSearchCoordinator.ExitReason.ABANDONED_RESULTS);
                        });

        // Search for 'Privacy Guide'.
        int titleId = R.string.privacy_guide_pref_title;
        String query = activity.getString(titleId);
        performSearch(query);

        // Do not click on search results but just exit -> emit "abandoned-results"
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getSearchCoordinator().exitSearchState(/* clearFragment= */ true);
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testHistograms_abandonedNoResults() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity activity = waitForSettingsActivity();

        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newSingleRecordWatcher(
                                    "Settings.Search.ExitReason",
                                    SettingsSearchCoordinator.ExitReason.ABANDONED_NORESULTS);
                        });
        performSearch("xzvfl"); // search returns no results for this gibberish

        // Just exit when there's no result -> emit "abandoned-no-results"
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getSearchCoordinator().exitSearchState(/* clearFragment= */ true);
                    histograms.assertExpected();
                });
    }
}
