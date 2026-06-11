// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.autofill.autofill_ai.utils.TestUtils.buildMercedezVehicleWithLabels;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.MockitoHelper;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;

/** Tests for {@link AutofillTravelFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT,
})
public class AutofillTravelFragmentTest {
    @Rule
    public SettingsActivityTestRule<AutofillTravelFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillTravelFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private EntityDataManager mEntityDataManager;
    @Mock private ReauthenticatorBridge mMockReauthenticatorBridge;

    @Before
    public void setUp() {
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
        ReauthenticatorBridge.setInstanceForTesting(mMockReauthenticatorBridge);

        mEntityDataManager = mock(EntityDataManager.class);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);
        when(mEntityDataManager.getInstancesToList()).thenReturn(new LinkedHashMap<>());
        when(mEntityDataManager.getEntitiesWithLabels()).thenReturn(Collections.emptyList());
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(true);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAiForType(anyInt())).thenReturn(true);
        when(mEntityDataManager.isEligibleToAutofillAiForType(anyInt())).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
    }

    @Test
    @MediumTest
    public void testScreenSetup() {
        mSettingsActivityTestRule.startSettingsActivity();

        AutofillTravelFragment fragment = mSettingsActivityTestRule.getFragment();
        assertThat(fragment.getPageTitle().get())
                .isEqualTo(
                        mSettingsActivityTestRule
                                .getActivity()
                                .getString(R.string.autofill_travel_title));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(fragment.getPreferenceScreen().shouldUseGeneratedIds()).isFalse();
                });
    }

    @Test
    @SmallTest
    public void testHelpMenuTriggersAutofillHelp() {
        SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.menu_id_targeted_help)).perform(click());

        verify(mHelpAndFeedbackLauncher)
                .show(
                        settingsActivity,
                        ContextUtils.getApplicationContext()
                                .getString(R.string.help_context_autofill),
                        /* url= */ null);
    }

    @Test
    @SmallTest
    public void testSearchIndexWhenAllEnabled() {
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTravelFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mSettingsActivityTestRule.getFragment().getProfile());
                });

        verify(mSearchIndexDataMock, atLeastOnce())
                .addEntryForKey(
                        eq(AutofillTravelFragment.class.getName()),
                        eq(AutofillTravelFragment.PREF_OPT_IN_TOGGLE),
                        any(Integer.class),
                        any(Integer.class));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testSearchIndexEmptyWhenFeatureDisabled() {
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTravelFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mSettingsActivityTestRule.getFragment().getProfile());
                });

        verify(mSearchIndexDataMock, never()).addEntryForKey(any(), any(), anyInt(), anyInt());
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_renderedCorrectly() {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Tesla",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillTravelFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference vehicleCategory = fragment.findPreference("Vehicle");
                    Criteria.checkThat(
                            "Vehicle entity category should exist",
                            vehicleCategory,
                            Matchers.notNullValue());
                    Preference vehicleEntity = fragment.findPreference("guid1");
                    Criteria.checkThat(
                            "Vehicle entity should exist", vehicleEntity, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Vehicle summary should match",
                            vehicleEntity.getSummary(),
                            Matchers.is("Tesla"));
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnSuccessfulReauth() {
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                TestUtils.getVehicleEntityType(),
                Arrays.asList(buildMercedezVehicleWithLabels("guid1")));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(TestUtils.getVehicleEntityType())
                        .setGuid("guid1")
                        .setRecordType(
                                org.chromium.components.autofill.autofill_ai.RecordType.LOCAL)
                        .setUseCount(0)
                        .setRequiresReauthToSee(true)
                        .build();

        when(mEntityDataManager.getEntityInstance("guid1")).thenReturn(entityInstance);

        mSettingsActivityTestRule.startSettingsActivity();

        Preference vehicleEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));

        ThreadUtils.runOnUiThreadBlocking(vehicleEntity::performClick);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        ThreadUtils.runOnUiThreadBlocking(() -> callbackCaptor.getValue().onResult(true));

        onView(withText("Edit Vehicle")).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnAddClickForLocalEntity() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);

        mSettingsActivityTestRule.startSettingsActivity();
        setTravelTogglePreference(true);

        Preference addVehicle =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceCategory category =
                                    mSettingsActivityTestRule
                                            .getFragment()
                                            .findPreference("Vehicle");
                            return category.findPreference("Vehicle" + " Add");
                        });
        assertNotNull(addVehicle);
        ThreadUtils.runOnUiThreadBlocking(addVehicle::performClick);

        onView(withText("Add Vehicle")).check(matches(isDisplayed()));

        onView(withText("Done")).perform(click());
        verify(mEntityDataManager)
                .addOrUpdateEntityInstance(
                        any(),
                        eq(R.string.autofill_ai_save_or_update_local_entity_source_notice),
                        eq(R.string.done),
                        any());
    }

    @Test
    @MediumTest
    public void testToggle_correctStateWhenTurnedOff() {
        mSettingsActivityTestRule.startSettingsActivity();
        setTravelTogglePreference(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSwitchPreference toggle =
                            mSettingsActivityTestRule
                                    .getFragment()
                                    .findPreference(AutofillTravelFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isPersistent()).isFalse();
                    assertThat(toggle.isVisible()).isTrue();
                    assertThat(toggle.isEnabled()).isTrue();
                    assertThat(toggle.isChecked()).isFalse();
                });
    }

    @Test
    @MediumTest
    public void testToggle_correctStateWhenTurnedOn() {
        mSettingsActivityTestRule.startSettingsActivity();
        setTravelTogglePreference(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSwitchPreference toggle =
                            mSettingsActivityTestRule
                                    .getFragment()
                                    .findPreference(AutofillTravelFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isVisible()).isTrue();
                    assertThat(toggle.isEnabled()).isTrue();
                    assertThat(toggle.isChecked()).isTrue();
                });
    }

    @Test
    @MediumTest
    public void testToggleDisabled_whenAutofillAiSettingsDisabled() {
        when(mEntityDataManager.canEnableOrDisableAutofillAiForType(anyInt())).thenReturn(false);
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTravelFragment fragment = mSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference toggle =
                            fragment.findPreference(AutofillTravelFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isVisible()).isTrue();
                    assertThat(toggle.isEnabled()).isFalse();
                    assertThat(toggle.isChecked()).isFalse();
                });
    }

    private void setTravelTogglePreference(boolean value) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(mSettingsActivityTestRule.getFragment().getProfile())
                                .setBoolean(Pref.AUTOFILL_AI_TRAVEL_ENTITIES_ENABLED, value));
    }
}
