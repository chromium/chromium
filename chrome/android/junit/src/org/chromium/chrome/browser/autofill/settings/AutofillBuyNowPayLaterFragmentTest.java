// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.preference.PreferenceScreen;

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
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.BnplIssuerForSettings;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/** JUnit tests of the class {@link AutofillBuyNowPayLaterFragment} */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillBuyNowPayLaterFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FragmentScenario<AutofillBuyNowPayLaterFragment> mScenario;
    private AutofillBuyNowPayLaterFragment mAutofillBuyNowPayLaterFragment;
    private ChromeSwitchPreference mBnplToggle;
    private ChromeBasePreference mBnplIssuerPref;
    private PreferenceScreen mScreen;
    private UserActionTester mActionTester;

    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private Menu mHelpMenu;
    @Mock private MenuItem mHelpItem;

    private static final String AFFIRM_DISPLAY_NAME = "Affirm";
    private static final Long INSTRUMENT_ID = 123L;

    @Before
    public void setUp() {
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        mActionTester = new UserActionTester();
        when(mPersonalDataManager.getBnplIssuersForSettings())
                .thenReturn(new BnplIssuerForSettings[0]);
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        mActionTester.tearDown();
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
                    mScreen = fragment.getPreferenceScreen();
                    mBnplToggle =
                            (ChromeSwitchPreference)
                                    fragment.findPreference(
                                            AutofillBuyNowPayLaterFragment
                                                    .PREF_KEY_ENABLE_BUY_NOW_PAY_LATER);
                    mBnplIssuerPref =
                            (ChromeBasePreference)
                                    fragment.findPreference(
                                            AutofillBuyNowPayLaterFragment
                                                    .PREF_KEY_BNPL_ISSUER_TERM);
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

    @Test
    public void testHelpMenuTriggersAutofillHelp() {
        doReturn(mHelpItem)
                .when(mHelpMenu)
                .add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        doReturn(R.id.menu_id_targeted_help).when(mHelpItem).getItemId();
        launchAutofillBuyNowPayLaterFragment();
        mAutofillBuyNowPayLaterFragment.onCreateOptionsMenu(mHelpMenu, mock(MenuInflater.class));
        verify(mHelpMenu).clear();
        verify(mHelpItem).setIcon(R.drawable.ic_help_and_feedback);

        mAutofillBuyNowPayLaterFragment.onOptionsItemSelected(mHelpItem);

        verify(mHelpAndFeedbackLauncher)
                .show(
                        mAutofillBuyNowPayLaterFragment.getActivity(),
                        ContextUtils.getApplicationContext()
                                .getString(R.string.help_context_autofill),
                        /* url= */ null);
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

    // Test to verify that the preference for a BNPL issuer is correctly displayed with the right
    // attributes.
    @Test
    public void testBnplIssuerPreference_CorrectlyDisplays() {
        BnplIssuerForSettings issuer =
                new BnplIssuerForSettings(
                        /* iconId= */ R.drawable.bnpl_icon_generic,
                        /* instrumentId= */ INSTRUMENT_ID,
                        /* displayName= */ AFFIRM_DISPLAY_NAME);
        when(mPersonalDataManager.getBnplIssuersForSettings())
                .thenReturn(new BnplIssuerForSettings[] {issuer});
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(true);

        launchAutofillBuyNowPayLaterFragment();

        assertNotNull(mBnplIssuerPref);
        assertEquals(AFFIRM_DISPLAY_NAME, mBnplIssuerPref.getTitle().toString());
        assertNotNull(mBnplIssuerPref.getIcon());
        assertTrue(mBnplIssuerPref.isSelectable());
        assertEquals(
                R.layout.autofill_server_data_label, mBnplIssuerPref.getWidgetLayoutResource());
        assertEquals(
                AutofillBuyNowPayLaterFragment.PREF_KEY_BNPL_ISSUER_TERM, mBnplIssuerPref.getKey());

        String termsUrl =
                mBnplIssuerPref
                        .getExtras()
                        .getString(AutofillBuyNowPayLaterFragment.PREF_LIST_TERMS_URL);
        assertNotNull(termsUrl);
        assertEquals(
                AutofillUiUtils.getManagePaymentMethodUrlForInstrumentId(INSTRUMENT_ID), termsUrl);
    }

    // Test to verify that the preference for a BNPL issuer is shown when the toggle is enabled.
    @Test
    public void testBnplIssuerPreference_WhenToggleIsEnabled_IsShown() {
        BnplIssuerForSettings issuer =
                new BnplIssuerForSettings(
                        /* iconId= */ R.drawable.bnpl_icon_generic,
                        /* instrumentId= */ INSTRUMENT_ID,
                        /* displayName= */ AFFIRM_DISPLAY_NAME);
        when(mPersonalDataManager.getBnplIssuersForSettings())
                .thenReturn(new BnplIssuerForSettings[] {issuer});
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(true);

        launchAutofillBuyNowPayLaterFragment();

        assertNotNull(mBnplToggle);
        assertNotNull(mBnplIssuerPref);
        assertEquals(
                "The screen should have 2 preferences (toggle + issuer).",
                2,
                mScreen.getPreferenceCount());
    }

    // Test to verify that the preference for a BNPL issuer is not shown when the toggle is
    // disabled.
    @Test
    public void testBnplIssuerPreference_WhenToggleIsDisabled_IsNotShown() {
        BnplIssuerForSettings issuer =
                new BnplIssuerForSettings(
                        /* iconId= */ R.drawable.bnpl_icon_generic,
                        /* instrumentId= */ INSTRUMENT_ID,
                        /* displayName= */ AFFIRM_DISPLAY_NAME);
        when(mPersonalDataManager.getBnplIssuersForSettings())
                .thenReturn(new BnplIssuerForSettings[] {issuer});
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(false);

        launchAutofillBuyNowPayLaterFragment();

        assertNotNull(mBnplToggle);
        assertNull(mBnplIssuerPref);
        assertEquals(
                "The screen should have only 1 preference (the toggle).",
                1,
                mScreen.getPreferenceCount());
    }

    // Test to verify that the user action is logged when the user clicks on the BNPL issuer terms.
    @Test
    public void testBnplIssuerPreference_IssuerTermsLinkClicked() {
        BnplIssuerForSettings issuer =
                new BnplIssuerForSettings(
                        /* iconId= */ R.drawable.bnpl_icon_generic,
                        /* instrumentId= */ INSTRUMENT_ID,
                        /* displayName= */ AFFIRM_DISPLAY_NAME);
        when(mPersonalDataManager.getBnplIssuersForSettings())
                .thenReturn(new BnplIssuerForSettings[] {issuer});
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(true);

        launchAutofillBuyNowPayLaterFragment();

        assertNotNull(mBnplToggle);
        assertNotNull(mBnplIssuerPref);

        mBnplIssuerPref.performClick();

        assertTrue(
                "User action should be logged when the user clicks on the BNPL issuer terms.",
                mActionTester
                        .getActions()
                        .contains(
                                AutofillBuyNowPayLaterFragment
                                        .BNPL_ISSUER_TERMS_CLICKED_USER_ACTION));
    }

    // Test to verify that the user action is logged when the user clicks on the BNPL toggle.
    @Test
    public void testBnplIssuerPreference_toggleClicked() {
        when(mPersonalDataManager.isBuyNowPayLaterEnabled()).thenReturn(true);
        launchAutofillBuyNowPayLaterFragment();
        assertNotNull(mBnplToggle);

        mBnplToggle.performClick();

        assertTrue(
                "User action should be logged when the user toggles BNPL off.",
                mActionTester
                        .getActions()
                        .contains(AutofillBuyNowPayLaterFragment.BNPL_TOGGLED_OFF_USER_ACTION));

        mBnplToggle.performClick();

        assertTrue(
                "User action should be logged when the user toggles BNPL on.",
                mActionTester
                        .getActions()
                        .contains(AutofillBuyNowPayLaterFragment.BNPL_TOGGLED_ON_USER_ACTION));
    }
}
