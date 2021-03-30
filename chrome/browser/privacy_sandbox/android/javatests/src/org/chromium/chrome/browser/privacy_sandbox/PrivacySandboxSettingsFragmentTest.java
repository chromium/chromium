// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertEquals;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Tests {@link PrivacySandboxSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS)
public final class PrivacySandboxSettingsFragmentTest {
    private static final String REFERRER_HISTOGRAM =
            "Settings.PrivacySandbox.PrivacySandboxReferrer";

    @Rule
    public SettingsActivityTestRule<PrivacySandboxSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySandboxSettingsFragment.class);

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testCreateActivityFromPrivacySettings() {
        int oldTotal = RecordHistogram.getHistogramTotalCountForTesting(REFERRER_HISTOGRAM);
        int oldPrivacy = RecordHistogram.getHistogramValueCountForTesting(
                REFERRER_HISTOGRAM, PrivacySandboxReferrer.PRIVACY_SETTINGS);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.PRIVACY_SETTINGS);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);

        int newTotal = RecordHistogram.getHistogramTotalCountForTesting(REFERRER_HISTOGRAM);
        int newPrivacy = RecordHistogram.getHistogramValueCountForTesting(
                REFERRER_HISTOGRAM, PrivacySandboxReferrer.PRIVACY_SETTINGS);
        assertEquals("Total histogram count increased by wrong value", 1, newTotal - oldTotal);
        assertEquals("Privacy referrer histogram count increased by wrong value", 1,
                newPrivacy - oldPrivacy);
    }

    @Test
    @SmallTest
    public void testCreateActivityFromCookiesSnackbar() {
        int oldTotal = RecordHistogram.getHistogramTotalCountForTesting(REFERRER_HISTOGRAM);
        int oldSnackbar = RecordHistogram.getHistogramValueCountForTesting(
                REFERRER_HISTOGRAM, PrivacySandboxReferrer.COOKIES_SNACKBAR);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.COOKIES_SNACKBAR);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
        int newTotal = RecordHistogram.getHistogramTotalCountForTesting(REFERRER_HISTOGRAM);
        int newSnackbar = RecordHistogram.getHistogramValueCountForTesting(
                REFERRER_HISTOGRAM, PrivacySandboxReferrer.COOKIES_SNACKBAR);
        assertEquals("Total histogram count increased by wrong value", 1, newTotal - oldTotal);
        assertEquals("Cookies snackbar referrer histogram count increased by wrong value", 1,
                newSnackbar - oldSnackbar);
    }
}
