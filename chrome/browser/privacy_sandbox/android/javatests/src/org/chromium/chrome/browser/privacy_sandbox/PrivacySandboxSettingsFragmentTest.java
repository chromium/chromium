// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/** Tests {@link PrivacySandboxSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
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

    private void openPrivacySandboxSettings(int privacySettings) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER, privacySettings);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
    }

    private boolean isPrivacySandboxEnabled() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(PrivacySandboxBridge::isPrivacySandboxEnabled);
    }

    @Test
    @SmallTest
    @DisabledTest(
            message = "crbug.com/1378703 - Test only applicable for superseded feature code paths")
    public void
    testChangeSetting() throws ExecutionException {
        Matcher<View> sandboxCheckboxMatcher = allOf(withId(R.id.switchWidget),
                withParent(withParent(hasDescendant(withText(R.string.privacy_sandbox_toggle)))));
        // Initially setting is on.
        openPrivacySandboxSettings(PrivacySandboxReferrer.PRIVACY_SETTINGS);
        onView(sandboxCheckboxMatcher).check(matches(isChecked()));
        assertTrue(isPrivacySandboxEnabled());

        // Turn sandbox off.
        onView(withText(R.string.privacy_sandbox_toggle)).perform(click());
        onView(sandboxCheckboxMatcher).check(matches(not(isChecked())));
        assertFalse(isPrivacySandboxEnabled());

        // Turn sandbox on.
        onView(withText(R.string.privacy_sandbox_toggle)).perform(click());
        onView(sandboxCheckboxMatcher).check(matches(isChecked()));
        assertTrue(isPrivacySandboxEnabled());
    }

    @Test
    @SmallTest
    public void testCreateActivityFromPrivacySettings() {
        openPrivacySandboxSettings(PrivacySandboxReferrer.PRIVACY_SETTINGS);

        assertEquals("Total histogram count wrong", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Privacy referrer histogram count", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.PRIVACY_SETTINGS));
    }

    @Test
    @SmallTest
    public void testCreateActivityFromCookiesSnackbar() {
        openPrivacySandboxSettings(PrivacySandboxReferrer.COOKIES_SNACKBAR);

        assertEquals("Total histogram count", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Cookies snackbar referrer histogram count wrong", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.COOKIES_SNACKBAR));
    }
}
