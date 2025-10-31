// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.os.Bundle;

import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/** JUnit tests of the class {@link AutofillBuyNowPayLaterFragment} */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillBuyNowPayLaterFragmentTest {
    private FragmentScenario<AutofillBuyNowPayLaterFragment> mScenario;

    private AutofillBuyNowPayLaterFragment mAutofillBuyNowPayLaterFragment;

    @Before
    public void setUp() {
        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillBuyNowPayLaterFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_Chromium_Settings);
        mScenario.onFragment(
                fragment -> {
                    assertNotNull(fragment.getPreferenceScreen());
                    mAutofillBuyNowPayLaterFragment = (AutofillBuyNowPayLaterFragment) fragment;
                });
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    // Test to verify that the Preference screen is displayed and its title is visible as expected.
    @Test
    public void testBuyNowPayLaterPreferenceScreen_shownWithTitle() {
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.autofill_bnpl_settings_label),
                mAutofillBuyNowPayLaterFragment.getPageTitle().get());
    }
}
