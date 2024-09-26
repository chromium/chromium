// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_SERVICE_IS_GOOGLE;
import static org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus.ANDROID_VERSION_TOO_OLD;
import static org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus.AVAILABLE;
import static org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus.NOT_ALLOWED_BY_POLICY;
import static org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus.UNKNOWN_ANDROID_AUTOFILL_SERVICE;
import static org.chromium.chrome.browser.autofill.AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_IS_READ_ONLY;

import android.text.SpannableString;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.StringRes;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.lifecycle.Lifecycle.Event;
import androidx.lifecycle.LifecycleRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Optional;

/** Unit tests for autofill options settings screen. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)
public class AutofillOptionsTest {

    private static final String SKIP_ALL_CHECKS_PARAM_VALUE = "skip_all_checks";
    private static final String ONLY_SKIP_AWG_CHECK_PARAM_VALUE = "only_skip_awg_check";

    // Shorthand for frequent enums that can't be static imports.
    private static final @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int DEFAULT =
            RadioButtonGroupThirdPartyPreference.ThirdPartyOption.DEFAULT;
    private static final @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int USE_3P =
            RadioButtonGroupThirdPartyPreference.ThirdPartyOption.USE_OTHER_PROVIDER;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private UserPrefsJni mMockUserPrefsJni;
    @Mock private PrefService mPrefs;
    @Mock private Profile mProfile;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private Runnable mRestartRunnable;
    @Mock private ModalDialogManager mDialogManager;

    @Captor ArgumentCaptor<PropertyModel> mRestartConfirmationDialogModelCaptor;

    private AutofillOptionsFragment mFragment;
    private AutoCloseable mCloseableMocks;
    private FragmentScenario mScenario;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsJni);
        doReturn(mPrefs).when(mMockUserPrefsJni).get(mProfile);
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);

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
        setAutofillAvailabilityToUseForTesting(AVAILABLE);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        // Initializing should set default property but not make use of dialogs or restarts yet.
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                        .initializeNow();

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertTrue(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getSpannableString(R.string.autofill_options_hint_3p_setting_ready));
    }

    @Test
    @SmallTest
    public void optionDisabledForAwgUpdatesOnResume() {
        setAutofillAvailabilityToUseForTesting(ANDROID_AUTOFILL_SERVICE_IS_GOOGLE);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        AutofillOptionsCoordinator autofillOptions =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail);
        PropertyModel model = autofillOptions.initializeNow();
        LifecycleRegistry lifecycleRegistry = new LifecycleRegistry(mFragment);
        autofillOptions.observeLifecycle(lifecycleRegistry);

        // On construction (assuming Awg is set), the setting is turned off and can't change.
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertFalse(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getSpannableString(R.string.autofill_options_hint_3p_setting_disabled));

        // On resume, check again whether AwG isn't used anymore — e.g. coming back from Settings.
        setAutofillAvailabilityToUseForTesting(AVAILABLE);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        lifecycleRegistry.handleLifecycleEvent(Event.ON_RESUME);

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertTrue(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getSpannableString(R.string.autofill_options_hint_3p_setting_ready));
    }

    @Test
    @SmallTest
    public void optionDisabledByPolicy() {
        setAutofillAvailabilityToUseForTesting(NOT_ALLOWED_BY_POLICY);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        AutofillOptionsCoordinator autofillOptions =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail);
        PropertyModel model = autofillOptions.initializeNow();

        // On construction (assuming Awg is set), the setting is turned off and can't change.
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertTrue(model.get(THIRD_PARTY_TOGGLE_IS_READ_ONLY));
        assertFalse(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getString(R.string.autofill_options_hint_policy));
    }

    @Test
    @SmallTest
    public void optionEnabledWithSpecialOverrideForAwg() {
        setAutofillAvailabilityToUseForTesting(ANDROID_AUTOFILL_SERVICE_IS_GOOGLE);
        addFeatureOverrideToSkipChecks(ONLY_SKIP_AWG_CHECK_PARAM_VALUE);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                        .initializeNow();

        // On construction (assuming Awg is set), the setting is turned off but may change.
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertTrue(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getSpannableString(R.string.autofill_options_hint_3p_setting_ready));
    }

    @Test
    @SmallTest
    public void overrideForAwgDoesntAllowOtherChecksToBeSkipped() {
        setAutofillAvailabilityToUseForTesting(ANDROID_VERSION_TOO_OLD);
        addFeatureOverrideToSkipChecks(ONLY_SKIP_AWG_CHECK_PARAM_VALUE);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                        .initializeNow();

        // On construction, the setting is turned off and can't change.
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertFalse(getRadioButtonComponent().isEnabled());
    }

    @Test
    @SmallTest
    public void genericOverrideAllowsOtherChecksToBeSkipped() {
        setAutofillAvailabilityToUseForTesting(UNKNOWN_ANDROID_AUTOFILL_SERVICE);
        addFeatureOverrideToSkipChecks(SKIP_ALL_CHECKS_PARAM_VALUE);

        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                        .initializeNow();

        // On construction, the setting is turned off but can change.
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertTrue(getRadioButtonComponent().isEnabled());
    }

    @Test
    @SmallTest
    public void optionEnabledWithGenericOverrideForAwg() {
        setAutofillAvailabilityToUseForTesting(ANDROID_AUTOFILL_SERVICE_IS_GOOGLE);
        addFeatureOverrideToSkipChecks(SKIP_ALL_CHECKS_PARAM_VALUE);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                        .initializeNow();

        // On construction (assuming Awg is set), the setting is turned off but may change.
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertTrue(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getSpannableString(R.string.autofill_options_hint_3p_setting_ready));
    }

    @Test
    @SmallTest
    public void optionEnabledToSwitchOffAwg() {
        setAutofillAvailabilityToUseForTesting(ANDROID_AUTOFILL_SERVICE_IS_GOOGLE);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                        .initializeNow();

        // On construction (assuming Awg is set), the setting is turned off but may change.
        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertFalse(model.get(THIRD_PARTY_TOGGLE_IS_READ_ONLY));
        assertTrue(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getSpannableString(R.string.autofill_options_hint_3p_setting_ready));
    }

    @Test
    @SmallTest
    public void toggledOptionRecordedInHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillOptionsMediator.HISTOGRAM_USE_THIRD_PARTY_FILLING, true);
        AutofillOptionsCoordinator autofillOptions =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable);
        PropertyModel model = autofillOptions.initializeNow();

        // Enabling the option should be recorded once.
        getRadioButtonComponent().getOptInButton().performClick();
        verifyAndDismissDialogManager(Optional.of(ButtonType.POSITIVE));
        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE), eq(true));
        histogramWatcher.assertExpected();

        // Enabling the option again should be ignored.
        model.get(ON_THIRD_PARTY_TOGGLE_CHANGED).onResult(true);
        histogramWatcher.assertExpected();

        // Disabling the option should be recorded again.
        reset(mDialogManager);
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillOptionsMediator.HISTOGRAM_USE_THIRD_PARTY_FILLING, false);
        getRadioButtonComponent().getDefaultButton().performClick();
        verifyAndDismissDialogManager(Optional.of(ButtonType.POSITIVE));
        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE), eq(false));
        histogramWatcher.assertExpected();

        verify(mRestartRunnable, times(2)).run(); // For enabling and disabling.
    }

    @Test
    @SmallTest
    public void updateSettingsFromPrefOnViewCreated() {
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        assertEquals(getRadioButtonComponent().getSelectedOption(), DEFAULT); // Not updated!

        // Update on initial binding. Fail if that triggers the dialog or restarting!
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

        verifyOptionReflectedInView(USE_3P);
    }

    @Test
    @SmallTest
    public void toggledOptionSetsPrefAndRestarts() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED)); // Not updated yet!

        getRadioButtonComponent().getOptInButton().performClick();

        verifyAndDismissDialogManager(Optional.of(ButtonType.POSITIVE));

        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE), eq(true));
        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(USE_3P);
        verify(mRestartRunnable).run();
    }

    @Test
    @SmallTest
    public void toggledOptionResetsWithoutConfirmation() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED)); // Not updated yet!

        getRadioButtonComponent().getOptInButton().performClick();

        verifyAndDismissDialogManager(Optional.of(ButtonType.NEGATIVE));

        verify(mPrefs, times(0))
                .setBoolean(eq(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE), anyBoolean());
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(DEFAULT);
        verify(mRestartRunnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void toggledOptionResetsWhenDismissed() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED)); // Not updated yet!

        getRadioButtonComponent().getOptInButton().performClick();

        verifyAndDismissDialogManager();

        verify(mPrefs, times(0))
                .setBoolean(eq(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE), anyBoolean());
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(DEFAULT);
        verify(mRestartRunnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void setPrefTogglesOptionOnResume() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        AutofillOptionsCoordinator autofillOptions =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail);
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
    public void suppliesTitle() {
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

        assertEquals(mFragment.getPageTitle().get(), getString(R.string.autofill_options_title));
    }

    @Test
    @SmallTest
    public void setsPref() {
        // Update on initial binding. Shouldn't trigger dialogs or restart.
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

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
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

        histogramWatcher.assertExpected();
    }

    private ModalDialogManager assertModalNotUsed() {
        fail("The modal dialog manager shouldn't have been used yet!");
        return null;
    }

    private String getString(@StringRes int stringId) {
        return mFragment.getResources().getString(stringId);
    }

    private SpannableString getSpannableString(@StringRes int stringId) {
        return SpanApplier.applySpans(
                getString(stringId),
                new SpanApplier.SpanInfo(
                        "<link>",
                        "</link>",
                        new NoUnderlineClickableSpan(
                                mFragment.getContext(), unusedView -> fail())));
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

    /** {@see verifyAndDismissDialogManager(Optional<Integer> optButtonToClick)} */
    private void verifyAndDismissDialogManager() {
        verifyAndDismissDialogManager(Optional.empty());
    }

    /**
     * Checks the mock was called. Captures the triggered dialog model and dismisses it. If given,
     * the it emulates a click on {@link optButtonToClick}. Otherwise, it calls {@code onDismiss}.
     *
     * @param optButtonToClick An optional containing a {@link ButtonType}.
     */
    private void verifyAndDismissDialogManager(Optional<Integer> optButtonToClick) {
        verify(mDialogManager)
                .showDialog(
                        mRestartConfirmationDialogModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mRestartConfirmationDialogModelCaptor.getValue();
        ModalDialogProperties.Controller mediator = model.get(ModalDialogProperties.CONTROLLER);
        if (optButtonToClick.isEmpty()) {
            mediator.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK);
            return;
        }
        assertTrue(optButtonToClick.isPresent());
        mediator.onClick(model, optButtonToClick.get());
        if (optButtonToClick.get() == ButtonType.NEGATIVE) {
            verify(mDialogManager)
                    .dismissDialog(eq(model), eq(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED));
            mediator.onDismiss(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    private RadioButtonGroupThirdPartyPreference getRadioButtonComponent() {
        assertNotNull(mFragment);
        return mFragment.getThirdPartyFillingOption();
    }

    private void assertHintDisplays(CharSequence message) {
        assertNotNull(mFragment);
        TextMessagePreference hint = mFragment.getHint();
        assertTrue(hint.isShown());
        assertNotNull(hint.getSummary());
        assertEquals(message.toString(), hint.getSummary().toString());
    }

    private void addFeatureOverrideToSkipChecks(String checksToSkip) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID,
                "skip_compatibility_check",
                checksToSkip);
        FeatureList.setTestValues(testValues);
    }
}
