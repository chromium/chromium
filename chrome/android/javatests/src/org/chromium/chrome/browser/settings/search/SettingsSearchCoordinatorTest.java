// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
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
}
