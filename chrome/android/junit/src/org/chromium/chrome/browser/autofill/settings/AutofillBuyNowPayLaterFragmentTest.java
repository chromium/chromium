// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;

import org.jspecify.annotations.NonNull;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/** JUnit tests of the class {@link AutofillBuyNowPayLaterFragment} */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillBuyNowPayLaterFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FragmentScenario<AutofillBuyNowPayLaterFragment> mScenario;
    private AutofillBuyNowPayLaterFragment mAutofillBuyNowPayLaterFragment;
    private ChromeSwitchPreference mBnplToggle;

    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    private void launchAutofillBuyNowPayLaterFragment() {
        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillBuyNowPayLaterFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_Chromium_Settings,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof AutofillBuyNowPayLaterFragment) {
                                    ((AutofillBuyNowPayLaterFragment) fragment)
                                            .setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment -> {
                    mAutofillBuyNowPayLaterFragment = (AutofillBuyNowPayLaterFragment) fragment;
                    mBnplToggle =
                            (ChromeSwitchPreference)
                                    fragment.findPreference(
                                            AutofillBuyNowPayLaterFragment
                                                    .PREF_KEY_ENABLE_BUY_NOW_PAY_LATER);
                });
    }

    // Test to verify that the Preference screen is displayed and its title is visible as expected.
    @Test
    public void testBuyNowPayLaterPreferenceScreen_shownWithTitle() {
        launchAutofillBuyNowPayLaterFragment();

        assertNotNull(mAutofillBuyNowPayLaterFragment.getPreferenceScreen());
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.autofill_bnpl_settings_label),
                mAutofillBuyNowPayLaterFragment.getPageTitle().get());
    }

    // Test to verify that the BNPL toggle is displayed on the Preference screen and its label and
    // sublabel are visible as expected.
    @Test
    public void testBuyNowPayLaterPreferenceScreen_ToggleIsShownWithLabelAndSubLabel() {
        launchAutofillBuyNowPayLaterFragment();

        assertNotNull(mBnplToggle);
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.autofill_bnpl_settings_label),
                mBnplToggle.getTitle());
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.autofill_bnpl_settings_toggle_sublabel),
                mBnplToggle.getSummary());
    }

    // Test to verify that when the `isBuyNowPayLaterEnabled` returns true, the BNPL toggle
    // is in the checked state.
    @Test
    public void testBuyNowPayLaterPreferenceScreen_WhenBuyNowPayLaterIsEnabled_ToggleIsChecked() {
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(true);

        launchAutofillBuyNowPayLaterFragment();

        assertTrue(mBnplToggle.isChecked());
    }

    // Test to verify that when the `isBuyNowPayLaterEnabled` returns false, the BNPL toggle
    // is in the unchecked state.
    @Test
    public void
            testBuyNowPayLaterPreferenceScreen_WhenBuyNowPayLaterIsDisabled_ToggleIsUnchecked() {
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(false);

        launchAutofillBuyNowPayLaterFragment();

        assertFalse(mBnplToggle.isChecked());
    }

    // Test to verify that when the BNPL toggle is clicked, the `setBuyNowPayLater` method is
    // called to set the BNPL status accordingly.
    @Test
    public void
            testBuyNowPayLaterPreferenceScreen_WhenToggleIsClicked_SetsEnableStatusAccordingly() {
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(true);
        launchAutofillBuyNowPayLaterFragment();
        assertTrue(mBnplToggle.isChecked());

        mBnplToggle.performClick();

        assertFalse(mBnplToggle.isChecked());
        verify(mPersonalDataManager).setBuyNowPayLater(eq(false));

        mBnplToggle.performClick();

        assertTrue(mBnplToggle.isChecked());
        verify(mPersonalDataManager).setBuyNowPayLater(eq(true));
    }
}
