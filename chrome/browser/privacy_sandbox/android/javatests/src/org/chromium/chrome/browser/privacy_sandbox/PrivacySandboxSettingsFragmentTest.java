// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertEquals;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Tests {@link PrivacySandboxSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class PrivacySandboxSettingsFragmentTest {
    private static final String REFERRER_HISTOGRAM =
            "Settings.PrivacySandbox.PrivacySandboxReferrer";

    @Rule
    public SettingsActivityTestRule<PrivacySandboxSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySandboxSettingsFragment.class);

    @Rule
    public HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @BeforeClass
    public static void beforeClass() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testCreateActivityFromPrivacySettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.PRIVACY_SETTINGS);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);

        assertEquals("Total histogram count wrong", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Privacy referrer histogram count", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.PRIVACY_SETTINGS));
    }

    @Test
    @SmallTest
    public void testCreateActivityFromCookiesSnackbar() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.COOKIES_SNACKBAR);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);

        assertEquals("Total histogram count", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Cookies snackbar referrer histogram count wrong", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.COOKIES_SNACKBAR));
    }
}
