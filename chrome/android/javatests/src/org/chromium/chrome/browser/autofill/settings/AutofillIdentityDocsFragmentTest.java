// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.autofill.autofill_ai.utils.TestUtils.buildMercedezVehicleWithLabels;
import static org.chromium.components.autofill.autofill_ai.utils.TestUtils.getPassportEntityType;
import static org.chromium.components.autofill.autofill_ai.utils.TestUtils.getVehicleEntityType;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroup;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.GoogleWalletLauncher;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager.EntityDataManagerObserver;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.MockitoHelper;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;

/** Tests for {@link AutofillIdentityDocsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT,
})
public class AutofillIdentityDocsFragmentTest {
    @Rule
    public SettingsActivityTestRule<AutofillIdentityDocsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillIdentityDocsFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private ReauthenticatorBridge mMockReauthenticatorBridge;

    @Mock private EntityDataManager mEntityDataManager;

    @Before
    public void setUp() {
        Intents.init();
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
        ReauthenticatorBridge.setInstanceForTesting(mMockReauthenticatorBridge);

        mEntityDataManager = mock(EntityDataManager.class);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);
        when(mEntityDataManager.getInstancesToList()).thenReturn(new LinkedHashMap<>());
        when(mEntityDataManager.getEntitiesWithLabels()).thenReturn(Collections.emptyList());
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAiForType(anyInt())).thenReturn(true);
        when(mEntityDataManager.isEligibleToAutofillAiForType(anyInt())).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        when(mMockReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
    }

    @After
    public void tearDown() {
        Intents.release();
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
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER
                            .updateDynamicPreferences(
                                    mSettingsActivityTestRule.getActivity(),
                                    mSearchIndexDataMock,
                                    mSettingsActivityTestRule.getFragment().getProfile());
                });

        verify(mSearchIndexDataMock, atLeastOnce())
                .addEntryForKey(
                        eq(AutofillIdentityDocsFragment.class.getName()),
                        eq(AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE),
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
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER
                            .updateDynamicPreferences(
                                    mSettingsActivityTestRule.getActivity(),
                                    mSearchIndexDataMock,
                                    mSettingsActivityTestRule.getFragment().getProfile());
                });

        verify(mSearchIndexDataMock, never())
                .addEntryForKey(
                        eq(AutofillIdentityDocsFragment.class.getName()),
                        eq(AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE),
                        any(Integer.class),
                        any(Integer.class));
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_identityDocsOnly() {
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                getPassportEntityType(),
                Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));
        instancesMap.put(
                getVehicleEntityType(), Arrays.asList(buildMercedezVehicleWithLabels("guid2")));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    assertNotNull(fragment.findPreference("guid1"));
                    assertNull(
                            "Vehicle entity should NOT be visible in Identity Docs",
                            fragment.findPreference("guid2"));
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_renderedCorrectly() {
        EntityType nationalIdType = TestUtils.getNationalIdEntityType();

        EntityInstanceWithLabels entity2 =
                new EntityInstanceWithLabels(
                        "guid2",
                        nationalIdType,
                        /* entityInstanceLabel= */ "National ID",
                        /* entityInstanceSubLabel= */ "California",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                getPassportEntityType(),
                Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));
        instancesMap.put(nationalIdType, Arrays.asList(entity2));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference passportCategory = fragment.findPreference("Passport");
                    Criteria.checkThat(
                            "Passport entity category should exist",
                            passportCategory,
                            Matchers.notNullValue());
                    PreferenceGroup passportGroup = (PreferenceGroup) passportCategory;
                    Preference passportEntity = fragment.findPreference("guid1");
                    Criteria.checkThat(
                            "Passport entity should exist",
                            passportEntity,
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            "Passport summary should match",
                            passportEntity.getSummary(),
                            Matchers.is("Germany"));
                    // Correct key check
                    Preference addPassport = passportGroup.findPreference("Passport Add");
                    Criteria.checkThat(
                            "Add passport button should exist in category",
                            addPassport,
                            Matchers.notNullValue());

                    Preference nationalIdCategory = fragment.findPreference("National ID");
                    Criteria.checkThat(
                            "National ID entity category should exist",
                            nationalIdCategory,
                            Matchers.notNullValue());
                    PreferenceGroup nationalIdGroup = (PreferenceGroup) nationalIdCategory;
                    Preference nationalIdEntity = fragment.findPreference("guid2");
                    Criteria.checkThat(
                            "National ID entity should exist",
                            nationalIdEntity,
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            "National ID summary should match",
                            nationalIdEntity.getSummary(),
                            Matchers.is("California"));
                    // Correct key check
                    Preference addNationalId = nationalIdGroup.findPreference("National ID Add");
                    Criteria.checkThat(
                            "Add National ID button should exist in category",
                            addNationalId,
                            Matchers.notNullValue());
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_addButtonNotEnabledWhenPassportDisabled() {
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                getPassportEntityType(),
                Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.canEnableOrDisableAutofillAiForType(EntityTypeName.PASSPORT))
                .thenReturn(false);

        mSettingsActivityTestRule.startSettingsActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference passportCategory = fragment.findPreference("Passport");
                    Criteria.checkThat(
                            "Passport entity category should exist",
                            passportCategory,
                            Matchers.notNullValue());
                    PreferenceGroup passportGroup = (PreferenceGroup) passportCategory;
                    Preference passportEntity = fragment.findPreference("guid1");
                    Criteria.checkThat(
                            "Passport entity should exist",
                            passportEntity,
                            Matchers.notNullValue());

                    Preference addPassport = passportGroup.findPreference("Passport Add");
                    assertThat(addPassport).isNotNull();
                    assertThat(addPassport.isEnabled()).isFalse();
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_notRenderedIfDisabledAndEmpty() {
        EntityType disabledType =
                getPassportEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ false,
                        /* isEligibleForWalletStorage= */ false);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(disabledType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference category =
                            fragment.findPreference(disabledType.getTypeNameAsString());
                    Criteria.checkThat(
                            "Disabled empty category should NOT exist",
                            category,
                            Matchers.nullValue());
                    assertNull(fragment.findPreference("Passport Add"));
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_notRenderedIfReadOnlyAndEmpty() {
        EntityType readOnlyType =
                getPassportEntityType(
                        /* isReadOnly= */ true,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ false);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(readOnlyType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference category =
                            fragment.findPreference(readOnlyType.getTypeNameAsString());
                    Criteria.checkThat(
                            "ReadOnly empty category should NOT exist",
                            category,
                            Matchers.nullValue());
                    assertNull(fragment.findPreference("Passport Add"));
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_renderedIfDisabledButNotEmpty() {
        EntityType disabledType =
                getPassportEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ false,
                        /* isEligibleForWalletStorage= */ false);

        EntityInstanceWithLabels entity =
                new EntityInstanceWithLabels(
                        "guid1",
                        disabledType,
                        "Label",
                        "Sublabel",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(disabledType, Arrays.asList(entity));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference category =
                            fragment.findPreference(disabledType.getTypeNameAsString());
                    Criteria.checkThat(
                            "Disabled NOT empty category should exist",
                            category,
                            Matchers.notNullValue());
                    PreferenceGroup group = (PreferenceGroup) category;
                    assertNotNull(group.findPreference("guid1"));
                    assertNull(group.findPreference("Passport Add"));
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_rebuildsOnEntityChange() throws Exception {
        EntityType passportType = getPassportEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap1 =
                new LinkedHashMap<>();
        instancesMap1.put(
                passportType, Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap1);

        mSettingsActivityTestRule.startSettingsActivity();

        // Capture the observer registered by the fragment.
        ArgumentCaptor<EntityDataManagerObserver> captor =
                ArgumentCaptor.forClass(EntityDataManagerObserver.class);
        verify(mEntityDataManager, atLeastOnce()).registerDataObserver(captor.capture());
        EntityDataManagerObserver observer = captor.getValue();

        // Initially check that the entity is rendered.
        CriteriaHelper.pollUiThread(
                () -> {
                    Preference passportEntity =
                            mSettingsActivityTestRule.getFragment().findPreference("guid1");
                    Criteria.checkThat(
                            "Passport entity should exist",
                            passportEntity,
                            Matchers.notNullValue());
                });

        // Change the entities and notify the observer.
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap2 =
                new LinkedHashMap<>();
        instancesMap2.put(passportType, Collections.emptyList());
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap2);
        ThreadUtils.runOnUiThreadBlocking(() -> observer.onEntityInstancesChanged());

        // Verify that the entity is gone.
        CriteriaHelper.pollUiThread(
                () -> {
                    Preference passportEntity =
                            mSettingsActivityTestRule.getFragment().findPreference("guid1");
                    Criteria.checkThat(
                            "Passport entity should no longer exist",
                            passportEntity,
                            Matchers.nullValue());
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnAddClick() {
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(getPassportEntityType(), Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();
        setIdentityTogglePreference(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceGroup group = (PreferenceGroup) fragment.findPreference("Passport");
                    Preference addBtn = group.findPreference("Passport Add");
                    addBtn.performClick();
                });

        onView(withText("Add passport")).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testToggle_correctStateWhenTurnedOff() {
        mSettingsActivityTestRule.startSettingsActivity();
        setIdentityTogglePreference(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSwitchPreference toggle =
                            mSettingsActivityTestRule
                                    .getFragment()
                                    .findPreference(
                                            AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isPersistent()).isFalse();
                    assertThat(toggle.isVisible()).isTrue();
                    assertThat(toggle.isEnabled()).isTrue();
                    assertThat(toggle.isChecked()).isFalse();
                });

        onView(enterpriseTextMatcher()).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testToggle_correctStateWhenTurnedOn() {
        mSettingsActivityTestRule.startSettingsActivity();
        setIdentityTogglePreference(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSwitchPreference toggle =
                            mSettingsActivityTestRule
                                    .getFragment()
                                    .findPreference(
                                            AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isVisible()).isTrue();
                    assertThat(toggle.isEnabled()).isTrue();
                    assertThat(toggle.isChecked()).isTrue();
                });

        onView(enterpriseTextMatcher()).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testToggleDisabled_whenAutofillAiSettingsDisabled() {
        when(mEntityDataManager.canEnableOrDisableAutofillAiForType(anyInt())).thenReturn(false);
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference toggle =
                            fragment.findPreference(
                                    AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isVisible()).isTrue();
                    assertThat(toggle.isEnabled()).isFalse();
                    assertThat(toggle.isChecked()).isFalse();
                });
        onView(enterpriseTextMatcher()).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testToggleManagedByPolicy() {
        when(mEntityDataManager.getIsAutofillAiDisabledByEnterprisePolicy()).thenReturn(true);
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference toggle =
                            fragment.findPreference(
                                    AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isEnabled()).isFalse();
                    assertThat(toggle.isChecked()).isFalse();
                });

        onView(enterpriseTextMatcher()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testScreenSetup() {
        mSettingsActivityTestRule.startSettingsActivity();

        AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
        assertThat(fragment.getPageTitle().get())
                .isEqualTo(
                        mSettingsActivityTestRule
                                .getActivity()
                                .getString(R.string.autofill_identity_docs_title));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(fragment.getPreferenceScreen().shouldUseGeneratedIds()).isFalse();
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnSuccessfulReauth() {
        EntityType passportType = getPassportEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                passportType, Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(passportType)
                        .setGuid("guid1")
                        .setRecordType(RecordType.LOCAL)
                        .setUseCount(0)
                        .setRequiresReauthToSee(true)
                        .build();

        when(mEntityDataManager.getEntityInstance("guid1")).thenReturn(entityInstance);

        mSettingsActivityTestRule.startSettingsActivity();

        Preference passportEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));

        // Click entity and capture reauth callback.
        ThreadUtils.runOnUiThreadBlocking(passportEntity::performClick);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        // Simulate successful reauth.
        ThreadUtils.runOnUiThreadBlocking(() -> callbackCaptor.getValue().onResult(true));

        onView(withText("Edit passport")).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_doesNotOpenEditorOnFailedReauth() {
        EntityType passportType = getPassportEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                passportType, Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(passportType)
                        .setGuid("guid1")
                        .setRecordType(
                                org.chromium.components.autofill.autofill_ai.RecordType.LOCAL)
                        .setUseCount(0)
                        .setRequiresReauthToSee(true)
                        .build();

        when(mEntityDataManager.getEntityInstance("guid1")).thenReturn(entityInstance);

        mSettingsActivityTestRule.startSettingsActivity();

        Preference passportEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));

        // Click entity and capture reauth callback.
        ThreadUtils.runOnUiThreadBlocking(passportEntity::performClick);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        // Simulate failed reauth.
        ThreadUtils.runOnUiThreadBlocking(() -> callbackCaptor.getValue().onResult(false));

        // Editor should NOT be open.
        onView(withText("Edit passport")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_SHOW_WALLET_DISABLED_BANNER)
    public void testDisabledWalletDataSharingDataCard_shownWhenDisabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(false);
        when(mEntityDataManager.canShowWalletDataSharingPromotion()).thenReturn(true);

        mSettingsActivityTestRule.startSettingsActivity();
        AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();

        assertNotNull(fragment.findPreference(AutofillAiDelegate.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    public void testDisabledWalletDataSharingDataCard_notShownWhenWalletPublicPassEnabled()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(true);

        mSettingsActivityTestRule.startSettingsActivity();
        AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();

        assertNull(fragment.findPreference(AutofillAiDelegate.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    public void testDisabledWalletDataSharingDataCard_notShownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(false);

        mSettingsActivityTestRule.startSettingsActivity();
        AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();

        assertNull(fragment.findPreference(AutofillAiDelegate.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_SHOW_WALLET_DISABLED_BANNER)
    public void testDisabledWalletDataSharingDataCard_notShownWhenFeatureDisabled()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(false);

        mSettingsActivityTestRule.startSettingsActivity();
        AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();

        assertNull(fragment.findPreference(AutofillAiDelegate.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    public void testDisabledSettingsText_linksToAutofillOptionsPage() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.card_button))
                .check(matches(withText(R.string.autofill_disable_settings_button_label)))
                .perform(scrollTo(), click());

        onView(withText(R.string.autofill_third_party_filling_default))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchIndex_addsSettingsInfoInThirdPartyMode() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });

        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER
                            .updateDynamicPreferences(
                                    mSettingsActivityTestRule.getActivity(),
                                    mSearchIndexDataMock,
                                    mSettingsActivityTestRule.getFragment().getProfile());
                });

        verify(mSearchIndexDataMock, atLeastOnce())
                .addEntryForKey(
                        eq(AutofillIdentityDocsFragment.class.getName()),
                        eq(AutofillAiDelegate.DISABLED_SETTINGS_INFO),
                        any(Integer.class),
                        any(Integer.class));
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnAddClick_eligibleForWalletFalse()
            throws Exception {
        EntityType passportType =
                TestUtils.getPassportEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ false);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);

        mSettingsActivityTestRule.startSettingsActivity();
        setIdentityTogglePreference(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PreferenceCategory category =
                            mSettingsActivityTestRule.getFragment().findPreference("Passport");
                    Preference addPassport = category.findPreference("Passport" + " Add");
                    assertNotNull(addPassport);
                    addPassport.performClick();
                });

        onView(withText("Add passport")).inRoot(isDialog()).check(matches(isDisplayed()));

        Context context = mSettingsActivityTestRule.getFragment().getContext();
        String expectedNoticeText =
                context.getString(R.string.autofill_ai_save_or_update_local_entity_source_notice);
        onView(withText(expectedNoticeText)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnAddClick_eligibleForWalletTrue()
            throws Exception {
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(true);
        EntityType passportType =
                TestUtils.getPassportEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ true);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);

        mSettingsActivityTestRule.startSettingsActivity();
        setIdentityTogglePreference(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PreferenceCategory category =
                            mSettingsActivityTestRule.getFragment().findPreference("Passport");
                    Preference addPassport = category.findPreference("Passport" + " Add");
                    assertNotNull(addPassport);
                    addPassport.performClick();
                });

        onView(withText("Add passport")).inRoot(isDialog()).check(matches(isDisplayed()));

        Context context = mSettingsActivityTestRule.getFragment().getContext();
        String walletTitle = context.getString(R.string.autofill_google_wallet_title);
        String expectedNoticeText =
                context.getString(
                                R.string.autofill_ai_save_or_update_entity_in_wallet_source_notice)
                        .replace("$1", walletTitle)
                        .replace("$2", walletTitle)
                        .replace("$3", TestAccounts.ACCOUNT1.getEmail())
                        .replace("<link>", "")
                        .replace("</link>", "");
        onView(withText(expectedNoticeText)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
    public void testAutofillAiEntities_opensWalletDefaultPage_whenUrlIsNull() throws Exception {
        EntityType passportType = TestUtils.getPassportEntityType();

        EntityInstanceWithLabels entity1 =
                TestUtils.buildGermanyPassportWithLabels(
                        "guid1", /* storedInWallet= */ true, /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        Preference passportEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(passportEntity);

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        // Since the walletEntityUrl is null, it should fallback to the general passes page.
        var intentMatcher =
                allOf(
                        hasAction(Intent.ACTION_VIEW),
                        hasData(Uri.parse(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL)));
        intending(intentMatcher).respondWith(result);

        ThreadUtils.runOnUiThreadBlocking(passportEntity::performClick);

        intended(intentMatcher);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
    public void testAutofillAiEntities_opensWalletPrivatePassPageOnClick() throws Exception {
        EntityType passportType = TestUtils.getPassportEntityType();

        String expectedUrl = "https://wallet.com/private";

        EntityInstanceWithLabels entity1 =
                TestUtils.buildGermanyPassportWithLabels(
                        "guid1", /* storedInWallet= */ true, expectedUrl);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        Preference passportEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(passportEntity);

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        var intentMatcher = allOf(hasAction(Intent.ACTION_VIEW), hasData(Uri.parse(expectedUrl)));
        intending(intentMatcher).respondWith(result);

        ThreadUtils.runOnUiThreadBlocking(passportEntity::performClick);

        intended(intentMatcher);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
    public void testAutofillAiEntities_opensWalletPrivatePassPageOnClick_featureDisabled()
            throws Exception {
        EntityType passportType = TestUtils.getPassportEntityType();

        EntityInstanceWithLabels entity1 =
                TestUtils.buildGermanyPassportWithLabels(
                        "guid1",
                        /* storedInWallet= */ true,
                        /* walletEntityUrl= */ "https://wallet.com/private");

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        Preference passportEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(passportEntity);

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        // Since the deep link feature is disabled, it should fallback to the general passes page.
        var intentMatcher =
                allOf(
                        hasAction(Intent.ACTION_VIEW),
                        hasData(Uri.parse(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL)));
        intending(intentMatcher).respondWith(result);

        ThreadUtils.runOnUiThreadBlocking(passportEntity::performClick);

        intended(intentMatcher);
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_addButtonNotEnabledWhenDisabledByPolicy() {
        when(mEntityDataManager.getIsAutofillAiDisabledByEnterprisePolicy()).thenReturn(true);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                getPassportEntityType(),
                Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();
        setIdentityTogglePreference(true);

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference passportCategory = fragment.findPreference("Passport");
                    Criteria.checkThat(
                            "Passport entity category should exist",
                            passportCategory,
                            Matchers.notNullValue());
                    PreferenceGroup passportGroup = (PreferenceGroup) passportCategory;
                    Preference passportEntity = fragment.findPreference("guid1");
                    Criteria.checkThat(
                            "Passport entity should exist",
                            passportEntity,
                            Matchers.notNullValue());

                    Preference addPassport = passportGroup.findPreference("Passport Add");
                    assertThat(addPassport).isNotNull();
                    assertThat(addPassport.isEnabled()).isFalse();
                });
    }

    @Test
    @MediumTest
    public void testThirdPartyMode_cardVisibleToggleAndAddButtonsDisabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                getPassportEntityType(),
                Arrays.asList(TestUtils.buildGermanyPassportWithLabels("guid1")));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();
        setIdentityTogglePreference(true);

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();

                    ChromeSwitchPreference toggle =
                            fragment.findPreference(
                                    AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isEnabled()).isFalse();
                    assertThat(toggle.isChecked()).isFalse();

                    PreferenceGroup group = (PreferenceGroup) fragment.findPreference("Passport");
                    Preference addPassport = group.findPreference("Passport Add");
                    assertThat(addPassport).isNotNull();
                    assertThat(addPassport.isEnabled()).isFalse();

                    Preference disabledSettingsCard =
                            fragment.findPreference(AutofillAiDelegate.DISABLED_SETTINGS_INFO);
                    assertThat(disabledSettingsCard.isVisible()).isTrue();
                });
    }

    private void setIdentityTogglePreference(boolean value) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(mSettingsActivityTestRule.getFragment().getProfile())
                                .setBoolean(Pref.AUTOFILL_AI_IDENTITY_ENTITIES_ENABLED, value));
    }

    private Matcher<View> enterpriseTextMatcher() {
        return allOf(
                withText(R.string.managed_by_your_organization),
                withEffectiveVisibility(Visibility.VISIBLE));
    }
}
