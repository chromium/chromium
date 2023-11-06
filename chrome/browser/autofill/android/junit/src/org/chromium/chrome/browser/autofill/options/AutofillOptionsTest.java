// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;

import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.StringRes;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.lifecycle.Lifecycle.Event;
import androidx.lifecycle.LifecycleRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for autofill options settings screen. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)
public class AutofillOptionsTest {
    // Shorthand for frequent enums that can't be static imports.
    private static final @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int DEFAULT =
            RadioButtonGroupThirdPartyPreference.ThirdPartyOption.DEFAULT;
    private static final @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int USE_3P =
            RadioButtonGroupThirdPartyPreference.ThirdPartyOption.USE_OTHER_PROVIDER;

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private UserPrefsJni mMockUserPrefsJni;
    @Mock private PrefService mPrefs;
    @Mock private Profile mProfile;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private AutofillOptionsFragment mFragment;
    private AutoCloseable mCloseableMocks;
    private FragmentScenario mScenario;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsJni);
        doReturn(mPrefs).when(mMockUserPrefsJni).get(mProfile);

        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillOptionsFragment.class,
                        AutofillOptionsFragment.createRequiredArgs(
                                AutofillOptionsReferrer.SETTINGS),
                        R.style.Theme_MaterialComponents);
        mScenario.onFragment(
                fragment -> {
                    mFragment =
                            (AutofillOptionsFragment)
                                    fragment; // Valid until scenario is recreated.
                    mFragment.setProfile(mProfile);
                    mFragment.setHelpAndFeedbackLauncher(mHelpAndFeedbackLauncher);
                });
    }

    @After
    public void tearDown() throws Exception {
        if (mScenario != null) {
            mScenario.close();
        }
        mCloseableMocks.close();
    }

    @Test
    @SmallTest
    public void constructedWithPrefAsDefaultForOption() {
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        PropertyModel model = new AutofillOptionsCoordinator(mFragment).initializeNow();

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
    }

    @Test
    @SmallTest
    public void toggledOptionRecordedInHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillOptionsMediator.HISTOGRAM_USE_THIRD_PARTY_FILLING, true);
        AutofillOptionsCoordinator autofillOptions = new AutofillOptionsCoordinator(mFragment);
        PropertyModel model = autofillOptions.initializeNow();

        // Enabling the option should be recorded once.
        getRadioButtonComponent().getOptInButton().performClick();
        histogramWatcher.assertExpected();

        // Enabling the option again should be ignored.
        model.get(ON_THIRD_PARTY_TOGGLE_CHANGED).onResult(true);
        histogramWatcher.assertExpected();

        // Disabling the option should be recorded again.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillOptionsMediator.HISTOGRAM_USE_THIRD_PARTY_FILLING, false);
        getRadioButtonComponent().getDefaultButton().performClick();
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void updateSettingsFromPrefOnViewCreated() {
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        assertEquals(getRadioButtonComponent().getSelectedOption(), DEFAULT); // Not updated!

        AutofillOptionsCoordinator.createFor(mFragment); // Initial binding updates the pref.

        verifyOptionReflectedInView(USE_3P);
    }

    @Test
    @SmallTest
    public void toggledOptionSetsPref() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        PropertyModel model = new AutofillOptionsCoordinator(mFragment).initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED)); // Not updated yet!

        getRadioButtonComponent().getOptInButton().performClick();

        verify(mPrefs).setBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE, true);
        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(USE_3P);
    }

    @Test
    @SmallTest
    public void setPrefTogglesOptionOnResume() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        AutofillOptionsCoordinator autofillOptions = new AutofillOptionsCoordinator(mFragment);
        PropertyModel model = autofillOptions.initializeNow();
        LifecycleRegistry lifecycleRegistry = new LifecycleRegistry(mFragment);
        autofillOptions.observeLifecycle(lifecycleRegistry);
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));

        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        lifecycleRegistry.handleLifecycleEvent(Event.ON_RESUME);

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(USE_3P);
    }

    @Test
    @SmallTest
    public void setsTitleAndPref() {
        AutofillOptionsCoordinator.createFor(mFragment); // Initial binding updates the pref.

        assertEquals(
                mFragment.getActivity().getTitle(), getString(R.string.autofill_options_title));
        assertEquals(
                getRadioButtonComponent().getKey(),
                AutofillOptionsFragment.PREF_AUTOFILL_THIRD_PARTY_FILLING);
        assertEquals(
                getRadioButtonComponent().getDefaultButton().getPrimaryText(),
                getString(R.string.autofill_third_party_filling_default));
        assertEquals(
                getRadioButtonComponent().getDefaultButton().getDescriptionText(),
                getString(R.string.autofill_third_party_filling_default_description));
        assertEquals(
                getRadioButtonComponent().getOptInButton().getPrimaryText(),
                getString(R.string.autofill_third_party_filling_opt_in));
        assertEquals(
                getRadioButtonComponent().getOptInButton().getDescriptionText(),
                getString(R.string.autofill_third_party_filling_opt_in_description));
    }

    @Test
    @SmallTest
    public void injectedHelpTriggersAutofillHelp() {
        Menu helpMenu = mock(Menu.class);
        MenuItem helpItem = mock(MenuItem.class);
        doReturn(helpItem)
                .when(helpMenu)
                .add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        doReturn(R.id.menu_id_targeted_help).when(helpItem).getItemId();

        // Create completely replaces the menu with only the help icon.
        mFragment.onCreateOptionsMenu(helpMenu, mock(MenuInflater.class));
        verify(helpMenu).clear();
        verify(helpItem).setIcon(R.drawable.ic_help_and_feedback);

        // Trigger the help as it would happen on tap.
        mFragment.onOptionsItemSelected(helpItem);
        verify(mHelpAndFeedbackLauncher)
                .show(mFragment.getActivity(), getString(R.string.help_context_autofill), null);
    }

    @Test
    @SmallTest
    public void passedReferrerRecordedInHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillOptionsMediator.HISTOGRAM_REFERRER,
                        AutofillOptionsReferrer.SETTINGS);

        // Component initialization triggers the recording.
        AutofillOptionsCoordinator.createFor(mFragment);

        histogramWatcher.assertExpected();
    }

    private String getString(@StringRes int stringId) {
        return mFragment.getResources().getString(stringId);
    }

    private void verifyOptionReflectedInView(
            @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int selectedOption) {
        assert selectedOption == DEFAULT || selectedOption == USE_3P;
        assertNotNull(getRadioButtonComponent());
        boolean uses_third_party = selectedOption == USE_3P;
        assertEquals(getRadioButtonComponent().getSelectedOption(), selectedOption);
        assertEquals(getRadioButtonComponent().getDefaultButton().isChecked(), !uses_third_party);
        assertEquals(getRadioButtonComponent().getOptInButton().isChecked(), uses_third_party);
    }

    private RadioButtonGroupThirdPartyPreference getRadioButtonComponent() {
        assertNotNull(mFragment);
        return mFragment.getThirdPartyFillingOption();
    }
}
