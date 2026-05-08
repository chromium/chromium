// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager.EntityDataManagerObserver;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.ui.test.util.MockitoHelper;

import java.time.LocalDate;
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
    ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT
})
public class AutofillIdentityDocsFragmentTest {
    @Rule
    public SettingsActivityTestRule<AutofillIdentityDocsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillIdentityDocsFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private Profile mProfileMock;
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER
                            .updateDynamicPreferences(
                                    mSettingsActivityTestRule.getActivity(),
                                    mSearchIndexDataMock,
                                    mProfileMock);
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER
                            .updateDynamicPreferences(
                                    mSettingsActivityTestRule.getActivity(),
                                    mSearchIndexDataMock,
                                    mProfileMock);
                });

        verify(mSearchIndexDataMock, never()).removeEntry(anyString());
        verify(mSearchIndexDataMock, never()).addEntryForKey(any(), any(), any(), any());
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_identityDocsOnly() {
        EntityType passportType = TestUtils.getPassportEntityType();
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        passportType,
                        /* entityInstanceLabel= */ "Passport",
                        /* entityInstanceSubLabel= */ "Germany",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        EntityInstanceWithLabels entity2 =
                new EntityInstanceWithLabels(
                        "guid2",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));
        instancesMap.put(vehicleType, Arrays.asList(entity2));

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
        EntityType passportType = TestUtils.getPassportEntityType();
        EntityType nationalIdType = TestUtils.getNationalIdEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        passportType,
                        /* entityInstanceLabel= */ "Passport",
                        /* entityInstanceSubLabel= */ "Germany",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

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
        instancesMap.put(passportType, Arrays.asList(entity1));
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
    public void testAutofillAiEntities_notRenderedIfDisabledAndEmpty() {
        EntityType disabledType =
                TestUtils.getPassportEntityType(
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
                TestUtils.getPassportEntityType(
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
                TestUtils.getPassportEntityType(
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

    // TODO(crbug.com/482994257): Sorting test

    @Test
    @MediumTest
    public void testAutofillAiEntities_rebuildsOnEntityChange() throws Exception {
        EntityType passportType = TestUtils.getPassportEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        passportType,
                        /* entityInstanceLabel= */ "Passport",
                        /* entityInstanceSubLabel= */ "John Doe",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap1 =
                new LinkedHashMap<>();
        instancesMap1.put(passportType, Arrays.asList(entity1));

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
        EntityType passportType = TestUtils.getPassportEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

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
    public void testToggleVisible_whenFeaturesEnabled() {
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference toggle =
                            fragment.findPreference(
                                    AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isVisible()).isTrue();
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testToggleHidden_whenFeatureDisabled() {
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference toggle =
                            fragment.findPreference(
                                    AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE);
                    assertNull("Toggle should NOT be added when feature disabled", toggle);
                });
    }

    @Test
    @MediumTest
    public void testTitle() {
        mSettingsActivityTestRule.startSettingsActivity();

        AutofillIdentityDocsFragment fragment = mSettingsActivityTestRule.getFragment();
        assertThat(fragment.getPageTitle().get())
                .isEqualTo(
                        mSettingsActivityTestRule
                                .getActivity()
                                .getString(R.string.autofill_identity_docs_title));
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnSuccessfulReauth() {
        EntityType passportType = TestUtils.getPassportEntityType();
        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        passportType,
                        /* entityInstanceLabel= */ "Passport",
                        /* entityInstanceSubLabel= */ "Germany",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(passportType)
                        .setGUID("guid1")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 12))
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
        EntityType passportType = TestUtils.getPassportEntityType();
        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        passportType,
                        /* entityInstanceLabel= */ "Passport",
                        /* entityInstanceSubLabel= */ "Germany",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(passportType)
                        .setGUID("guid1")
                        .setRecordType(
                                org.chromium.components.autofill.autofill_ai.RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 12))
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

    // TODO(crbug.com/482994257): Wallet tests

}
