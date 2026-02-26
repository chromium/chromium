// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus.AVAILABLE;
import static org.chromium.chrome.browser.autofill.AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_IS_READ_ONLY;

import android.content.ComponentName;
import android.content.Context;
import android.text.SpannableString;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.autofill.AutofillManager;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.autofill_ai.AutofillAiOptInStatus;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Unit tests for autofill options settings screen. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.AUTOFILL_THIRD_PARTY_MODE_RESTORED_ON_START})
@DisableFeatures({ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA})
public class AutofillOptionsTest {
    private static final String SKIP_ALL_CHECKS_PARAM_VALUE = "skip_all_checks";
    private static final String ONLY_SKIP_AWG_CHECK_PARAM_VALUE = "only_skip_awg_check";

    private static final ComponentName AWG_PACKAGE =
            new ComponentName(
                    "com.google.android.gms",
                    "com.google.android.gms.autofill.service.AutofillService");
    private static final ComponentName EXAMPLE_SERVICE_PACKAGE =
            new ComponentName("com.service.example", "com.service.example.autofill.service.One");
    private static final ComponentName OTHER_SERVICE_PACKAGE =
            new ComponentName("com.another.example", "com.another.example.autofill.service.Two");

    // Shorthand for frequent enums that can't be static imports.
    private static final @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int DEFAULT =
            RadioButtonGroupThirdPartyPreference.ThirdPartyOption.DEFAULT;
    private static final @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int USE_3P =
            RadioButtonGroupThirdPartyPreference.ThirdPartyOption.USE_OTHER_PROVIDER;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserPrefsJni mMockUserPrefsJni;
    @Mock private PrefService mPrefs;
    @Mock private Profile mProfile;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private Runnable mRestartRunnable;
    @Mock private ModalDialogManager mDialogManager;
    @Mock private AutofillManager mAutofillManager;
    @Mock private EntityDataManager mMockEntityDataManager;
    @Mock private ReauthenticatorBridge mMockReauthenticatorBridge;

    @Captor ArgumentCaptor<PropertyModel> mRestartConfirmationDialogModelCaptor;

    private AutofillOptionsFragment mFragment;
    private FragmentScenario mScenario;

    @Before
    public void setUp() {
        ReauthenticatorBridge.setInstanceForTesting(mMockReauthenticatorBridge);
        EntityDataManagerFactory.setInstanceForTesting(mMockEntityDataManager);
        UserPrefsJni.setInstanceForTesting(mMockUserPrefsJni);
        doReturn(mPrefs).when(mMockUserPrefsJni).get(mProfile);
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
        ShadowApplication shadowApplication = Shadow.extract(RuntimeEnvironment.getApplication());
        shadowApplication.setSystemService(Context.AUTOFILL_MANAGER_SERVICE, mAutofillManager);

        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillOptionsFragment.class,
                        AutofillOptionsFragment.createRequiredArgs(
                                AutofillOptionsReferrer.SETTINGS),
                        R.style.Theme_BrowserUI_DayNight);
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
    }

    @Test
    @SmallTest
    public void constructedWithPrefAsDefaultForOption() {
        doReturn(EXAMPLE_SERVICE_PACKAGE).when(mAutofillManager).getAutofillServiceComponentName();
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);

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
        doReturn(AWG_PACKAGE).when(mAutofillManager).getAutofillServiceComponentName();
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);

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
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        lifecycleRegistry.handleLifecycleEvent(Event.ON_RESUME);

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        assertTrue(getRadioButtonComponent().isEnabled());
        assertHintDisplays(getSpannableString(R.string.autofill_options_hint_3p_setting_ready));
    }

    @Test
    @SmallTest
    public void optionDisabledByPolicy() {
        doReturn(EXAMPLE_SERVICE_PACKAGE).when(mAutofillManager).getAutofillServiceComponentName();
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(false)
                .when(mPrefs)
                .getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);

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
    public void optionEnabledToSwitchOffAwg() {
        doReturn(AWG_PACKAGE).when(mAutofillManager).getAutofillServiceComponentName();
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);

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
        verifyAndDismissDialogManager(ButtonType.POSITIVE);
        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL), eq(true));
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
        verifyAndDismissDialogManager(ButtonType.POSITIVE);
        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL), eq(false));
        histogramWatcher.assertExpected();

        verify(mRestartRunnable, times(2)).run(); // For enabling and disabling.
    }

    @Test
    @SmallTest
    public void updateSettingsFromPrefOnViewCreated() {
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);
        assertEquals(DEFAULT, getRadioButtonComponent().getSelectedOption()); // Not updated!

        // Update on initial binding. Fail if that triggers the dialog or restarting!
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

        verifyOptionReflectedInView(USE_3P);
    }

    @Test
    @SmallTest
    public void toggledOptionSetsPrefAndRestarts() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillOptionsMediator.HISTOGRAM_RESTART_ACCEPTED, true);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED)); // Not updated yet!

        getRadioButtonComponent().getOptInButton().performClick();

        verifyAndDismissDialogManager(ButtonType.POSITIVE);

        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL), eq(true));
        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(USE_3P);
        histogramWatcher.assertExpected();
        verify(mRestartRunnable).run();
    }

    @Test
    @SmallTest
    public void toggledOptionResetsWithoutConfirmation() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillOptionsMediator.HISTOGRAM_RESTART_ACCEPTED, false);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED)); // Not updated yet!

        getRadioButtonComponent().getOptInButton().performClick();

        verifyAndDismissDialogManager(ButtonType.NEGATIVE);

        verify(mPrefs, times(0))
                .setBoolean(eq(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL), anyBoolean());
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(DEFAULT);
        histogramWatcher.assertExpected();
        verify(mRestartRunnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void toggledOptionResetsWhenDismissed() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED)); // Not updated yet!

        getRadioButtonComponent().getOptInButton().performClick();

        verifyAndDismissDialogManager();

        verify(mPrefs, times(0))
                .setBoolean(eq(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL), anyBoolean());
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(DEFAULT);
        verify(mRestartRunnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void setPrefTogglesOptionOnResume() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);
        // Toggling on resume is to align with prefs and shouldn't trigger restart/dialogs.
        AutofillOptionsCoordinator autofillOptions =
                new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail);
        PropertyModel model = autofillOptions.initializeNow();
        LifecycleRegistry lifecycleRegistry = new LifecycleRegistry(mFragment);
        autofillOptions.observeLifecycle(lifecycleRegistry);
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));

        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        lifecycleRegistry.handleLifecycleEvent(Event.ON_RESUME);

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
        verifyOptionReflectedInView(USE_3P);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void suppliesTitleWhenAutofillAiDisabled() {
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

        assertEquals(mFragment.getPageTitle().get(), getString(R.string.autofill_options_title));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void suppliesTitle() {
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

        assertEquals(mFragment.getPageTitle().get(), getString(R.string.autofill_settings_title));
    }

    @Test
    @SmallTest
    public void setsPref() {
        // Update on initial binding. Shouldn't trigger dialogs or restart.
        AutofillOptionsCoordinator.createFor(mFragment, this::assertModalNotUsed, Assert::fail);

        assertEquals(
                AutofillOptionsFragment.PREF_AUTOFILL_THIRD_PARTY_FILLING,
                getRadioButtonComponent().getKey());
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
        verify(helpItem).setIcon(R.drawable.ic_help_24dp);

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

    @Test
    @SmallTest
    public void toggledOptionStoresPackageNamePref() {
        doReturn(EXAMPLE_SERVICE_PACKAGE).when(mAutofillManager).getAutofillServiceComponentName();
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));

        getRadioButtonComponent().getOptInButton().performClick();
        verifyAndDismissDialogManager(ButtonType.POSITIVE);

        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL), eq(true));
        verify(mPrefs)
                .setString(
                        eq(Pref.AUTOFILL_THIRD_PARTY_PACKAGE_USED_FOR_PLATFORM_AUTOFILL),
                        eq(EXAMPLE_SERVICE_PACKAGE.flattenToString()));
        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
    }

    @Test
    @SmallTest
    public void toggledOptionResetsPackageNamePref() {
        doReturn(EXAMPLE_SERVICE_PACKAGE).when(mAutofillManager).getAutofillServiceComponentName();
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL);
        PropertyModel model =
                new AutofillOptionsCoordinator(mFragment, () -> mDialogManager, mRestartRunnable)
                        .initializeNow();
        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));

        getRadioButtonComponent().getDefaultButton().performClick();
        verifyAndDismissDialogManager(ButtonType.POSITIVE);

        verify(mPrefs).setBoolean(eq(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL), eq(false));
        verify(mPrefs)
                .setString(
                        eq(Pref.AUTOFILL_THIRD_PARTY_PACKAGE_USED_FOR_PLATFORM_AUTOFILL), eq(""));
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiFeatureVisibleWhenFeatureEnabled() {
        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();
        assertTrue(mFragment.getAutofillAiCategory().isVisible());
        assertTrue(mFragment.getAutofillServiceProviderCategory().isVisible());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiToggleEnabledWhenEligible() {
        doReturn(true).when(mMockEntityDataManager).isEligibleToAutofillAi();

        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();

        assertTrue(mFragment.getAutofillAiSwitch().isEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiToggleDisabledWhenNotEligible() {
        doReturn(false).when(mMockEntityDataManager).isEligibleToAutofillAi();

        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();

        assertFalse(mFragment.getAutofillAiSwitch().isEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiToggleSetsOptInStatus() {
        doReturn(true).when(mMockEntityDataManager).isEligibleToAutofillAi();
        doReturn(false).when(mMockEntityDataManager).getAutofillAiOptInStatus();
        doReturn(true).when(mMockEntityDataManager).setAutofillAiOptInStatus(anyInt());

        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();

        // Toggle the switch to ON.
        mFragment
                .getAutofillAiSwitch()
                .getOnPreferenceChangeListener()
                .onPreferenceChange(mFragment.getAutofillAiSwitch(), true);
        verify(mMockEntityDataManager).setAutofillAiOptInStatus(AutofillAiOptInStatus.OPTED_IN);

        // Toggle the switch to OFF.
        mFragment
                .getAutofillAiSwitch()
                .getOnPreferenceChangeListener()
                .onPreferenceChange(mFragment.getAutofillAiSwitch(), false);
        verify(mMockEntityDataManager).setAutofillAiOptInStatus(AutofillAiOptInStatus.OPTED_OUT);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiToggleResetsOnFailure() {
        doReturn(true).when(mMockEntityDataManager).isEligibleToAutofillAi();
        doReturn(false).when(mMockEntityDataManager).getAutofillAiOptInStatus();
        doReturn(false).when(mMockEntityDataManager).setAutofillAiOptInStatus(anyInt());

        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();

        // Toggle the switch to ON.
        mFragment
                .getAutofillAiSwitch()
                .getOnPreferenceChangeListener()
                .onPreferenceChange(mFragment.getAutofillAiSwitch(), true);

        // Since it failed, it should have been reset to match getAutofillAiOptInStatus() which is
        // false.
        assertFalse(mFragment.getAutofillAiSwitch().isChecked());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiToggleHiddenWhenFeatureDisabled() {
        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();
        assertFalse(mFragment.getAutofillAiCategory().isVisible());
        assertFalse(mFragment.getAutofillServiceProviderCategory().isVisible());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiToggleHiddenWhenDeepLinkOpened() {
        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillOptionsFragment.class,
                        AutofillOptionsFragment.createRequiredArgs(
                                AutofillOptionsReferrer.DEEP_LINK_TO_SETTINGS),
                        R.style.Theme_BrowserUI_DayNight);
        mScenario.onFragment(
                fragment -> {
                    mFragment =
                            (AutofillOptionsFragment)
                                    fragment; // Valid until scenario is recreated.
                    mFragment.setProfile(mProfile);
                });
        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();
        assertFalse(mFragment.getAutofillAiCategory().isVisible());
        assertFalse(mFragment.getAutofillServiceProviderCategory().isVisible());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiReauthToggleInitialValue() {
        doReturn(true)
                .when(mPrefs)
                .getBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA);

        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();

        assertTrue(mFragment.getAutofillAiAuthenticationSwitch().isChecked());

        doReturn(false)
                .when(mPrefs)
                .getBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA);
        mScenario.onFragment(
                fragment -> {
                    new AutofillOptionsCoordinator(
                                    mFragment, this::assertModalNotUsed, Assert::fail)
                            .initializeNow();
                });
        assertFalse(mFragment.getAutofillAiAuthenticationSwitch().isChecked());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiReauthToggleSuccessful() {
        doReturn(false)
                .when(mPrefs)
                .getBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA);
        doReturn(org.chromium.chrome.browser.device_reauth.BiometricStatus.BIOMETRICS_AVAILABLE)
                .when(mMockReauthenticatorBridge)
                .getBiometricAvailabilityStatus();

        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();

        // Toggle the switch to ON.
        mFragment
                .getAutofillAiAuthenticationSwitch()
                .getOnPreferenceChangeListener()
                .onPreferenceChange(mFragment.getAutofillAiAuthenticationSwitch(), true);

        // Verify reauth is triggered.
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        // Simulate successful reauth.
        doReturn(true)
                .when(mPrefs)
                .getBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA);
        callbackCaptor.getValue().onResult(true);

        // Verify pref is updated.
        verify(mPrefs).setBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA, true);
        assertTrue(mFragment.getAutofillAiAuthenticationSwitch().isChecked());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiReauthToggleFailed() {
        doReturn(true)
                .when(mPrefs)
                .getBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA);
        doReturn(org.chromium.chrome.browser.device_reauth.BiometricStatus.BIOMETRICS_AVAILABLE)
                .when(mMockReauthenticatorBridge)
                .getBiometricAvailabilityStatus();

        new AutofillOptionsCoordinator(mFragment, this::assertModalNotUsed, Assert::fail)
                .initializeNow();

        // Toggle the switch to OFF.
        mFragment
                .getAutofillAiAuthenticationSwitch()
                .getOnPreferenceChangeListener()
                .onPreferenceChange(mFragment.getAutofillAiAuthenticationSwitch(), false);

        // Verify reauth is triggered.
        ArgumentCaptor<org.chromium.base.Callback<Boolean>> callbackCaptor =
                ArgumentCaptor.forClass(org.chromium.base.Callback.class);
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        // Simulate failed reauth.
        callbackCaptor.getValue().onResult(false);

        // Verify pref is NOT updated.
        verify(mPrefs, times(0))
                .setBoolean(
                        eq(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA), anyBoolean());
        // Switch should be reset to ON.
        assertTrue(mFragment.getAutofillAiAuthenticationSwitch().isChecked());
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
                        new ChromeClickableSpan(mFragment.getContext(), unusedView -> fail())));
    }

    private void verifyOptionReflectedInView(
            @RadioButtonGroupThirdPartyPreference.ThirdPartyOption int selectedOption) {
        assertThat(selectedOption).isAnyOf(DEFAULT, USE_3P);
        assertNotNull(getRadioButtonComponent());
        boolean usesThirdParty = selectedOption == USE_3P;
        assertEquals(getRadioButtonComponent().getSelectedOption(), selectedOption);
        assertEquals(getRadioButtonComponent().getDefaultButton().isChecked(), !usesThirdParty);
        assertEquals(getRadioButtonComponent().getOptInButton().isChecked(), usesThirdParty);
    }

    /** {@see verifyAndDismissDialogManager(@Nullable Integer buttonToClick)} */
    private void verifyAndDismissDialogManager() {
        verifyAndDismissDialogManager(null);
    }

    /**
     * Checks the mock was called. Captures the triggered dialog model and dismisses it. If given,
     * the it emulates a click on {@link buttonToClick}. Otherwise, it calls {@code onDismiss}.
     *
     * @param buttonToClick An optional containing a {@link ButtonType}.
     */
    private void verifyAndDismissDialogManager(@Nullable Integer buttonToClick) {
        verify(mDialogManager)
                .showDialog(
                        mRestartConfirmationDialogModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mRestartConfirmationDialogModelCaptor.getValue();
        ModalDialogProperties.Controller mediator = model.get(ModalDialogProperties.CONTROLLER);
        if (buttonToClick == null) {
            mediator.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK);
            return;
        }
        mediator.onClick(model, buttonToClick);
        if (buttonToClick == ButtonType.NEGATIVE) {
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
}
