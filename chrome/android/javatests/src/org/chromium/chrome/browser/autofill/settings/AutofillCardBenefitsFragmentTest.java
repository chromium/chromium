// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for AutofillCardBenefitsFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE
})
public class AutofillCardBenefitsFragmentTest {
    @Rule public final AutofillTestRule mRule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillCardBenefitsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillCardBenefitsFragment.class);

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
    }

    // Test to verify that the Preference screen is displayed and its title is visible as expected.
    @Test
    @MediumTest
    public void testCardBenefitsPreferenceScreen_shownWithTitle() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNotNull(getPreferenceScreen(activity));
        Assert.assertEquals(
                activity.getTitle().toString(),
                activity.getString(R.string.autofill_card_benefits_settings_page_title));
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillCardBenefitsFragment) activity.getMainFragment()).getPreferenceScreen();
    }
}
