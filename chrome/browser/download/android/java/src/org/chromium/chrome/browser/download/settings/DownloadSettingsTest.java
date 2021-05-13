// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import androidx.preference.Preference;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Test for download settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DownloadSettingsTest {
    @Rule
    public final SettingsActivityTestRule<DownloadSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(DownloadSettings.class);

    private Preference assertPreference(final String preferenceKey) throws Exception {
        return assertPreference(preferenceKey, Matchers.notNullValue());
    }

    private Preference assertPreference(final String preferenceKey, Matcher<Object> matcher)
            throws Exception {
        DownloadSettings downloadSettings = mSettingsActivityTestRule.getFragment();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Expected valid preference for: " + preferenceKey,
                    downloadSettings.findPreference(preferenceKey), matcher);
        });

        return TestThreadUtils.runOnUiThreadBlocking(
                () -> downloadSettings.findPreference(preferenceKey));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.DOWNLOAD_LATER)
    public void testGeneralSettings() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        assertPreference(DownloadSettings.PREF_LOCATION_CHANGE);
        assertPreference(DownloadSettings.PREF_LOCATION_PROMPT_ENABLED);
        assertPreference(DownloadSettings.PREF_DOWNLOAD_LATER_PROMPT_ENABLED);
        assertPreference(DownloadSettings.PREF_PREFETCHING_ENABLED);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.DOWNLOAD_LATER)
    public void testWithoutDownloadLater() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        assertPreference(DownloadSettings.PREF_LOCATION_CHANGE);
        assertPreference(DownloadSettings.PREF_LOCATION_PROMPT_ENABLED);
        assertPreference(DownloadSettings.PREF_DOWNLOAD_LATER_PROMPT_ENABLED, Matchers.nullValue());
        assertPreference(DownloadSettings.PREF_PREFETCHING_ENABLED);
    }
}
