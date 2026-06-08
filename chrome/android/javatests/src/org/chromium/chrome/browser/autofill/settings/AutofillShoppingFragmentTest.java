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
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
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
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.MockitoHelper;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;

/** Tests for {@link AutofillShoppingFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT,
    ChromeFeatureList.AUTOFILL_AI_WALLET_SHOPPING
})
public class AutofillShoppingFragmentTest {
    @Rule
    public SettingsActivityTestRule<AutofillShoppingFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillShoppingFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private SettingsIndexData mSearchIndexDataMock;
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
        when(mEntityDataManager.canShowWalletDataSharingPromotion()).thenReturn(true);
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
                    AutofillShoppingFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mSettingsActivityTestRule.getFragment().getProfile());
                });

        verify(mSearchIndexDataMock, atLeastOnce())
                .addEntryForKey(
                        eq(AutofillShoppingFragment.class.getName()),
                        eq(AutofillShoppingFragment.PREF_OPT_IN_TOGGLE),
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
                    AutofillShoppingFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mSettingsActivityTestRule.getFragment().getProfile());
                });

        verify(mSearchIndexDataMock, never())
                .addEntryForKey(
                        eq(AutofillShoppingFragment.class.getName()),
                        eq(AutofillShoppingFragment.PREF_OPT_IN_TOGGLE),
                        any(Integer.class),
                        any(Integer.class));
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_shoppingOnly() {
        EntityType orderType = TestUtils.getOrderEntityType();
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        orderType,
                        /* entityInstanceLabel= */ "Order",
                        /* entityInstanceSubLabel= */ "Store",
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
        instancesMap.put(orderType, Arrays.asList(entity1));
        instancesMap.put(vehicleType, Arrays.asList(entity2));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillShoppingFragment fragment = mSettingsActivityTestRule.getFragment();
                    assertNotNull(fragment.findPreference("guid1"));
                    assertNull(
                            "Vehicle entity should NOT be visible in Shopping",
                            fragment.findPreference("guid2"));
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_renderedCorrectly() {
        EntityType orderType = TestUtils.getOrderEntityType();
        EntityType shipmentType = TestUtils.getShipmentEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        orderType,
                        /* entityInstanceLabel= */ "Order",
                        /* entityInstanceSubLabel= */ "Store",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        EntityInstanceWithLabels entity2 =
                new EntityInstanceWithLabels(
                        "guid2",
                        shipmentType,
                        /* entityInstanceLabel= */ "Shipment",
                        /* entityInstanceSubLabel= */ "Carrier",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(orderType, Arrays.asList(entity1));
        instancesMap.put(shipmentType, Arrays.asList(entity2));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        mSettingsActivityTestRule.startSettingsActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillShoppingFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference orderCategory = fragment.findPreference("Order");
                    Criteria.checkThat(
                            "Order entity category should exist",
                            orderCategory,
                            Matchers.notNullValue());
                    PreferenceGroup orderGroup = (PreferenceGroup) orderCategory;
                    Preference orderEntity = fragment.findPreference("guid1");
                    Criteria.checkThat(
                            "Order entity should exist", orderEntity, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Order summary should match",
                            orderEntity.getSummary(),
                            Matchers.is("Store"));
                    Preference addOrder = orderGroup.findPreference("Order Add");
                    Criteria.checkThat(
                            "Add order button should NOT exist in category",
                            addOrder,
                            Matchers.nullValue());

                    Preference shipmentCategory = fragment.findPreference("Shipment");
                    Criteria.checkThat(
                            "Shipment entity category should exist",
                            shipmentCategory,
                            Matchers.notNullValue());
                    PreferenceGroup shipmentGroup = (PreferenceGroup) shipmentCategory;
                    Preference shipmentEntity = fragment.findPreference("guid2");
                    Criteria.checkThat(
                            "Shipment entity should exist",
                            shipmentEntity,
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            "Shipment summary should match",
                            shipmentEntity.getSummary(),
                            Matchers.is("Carrier"));
                    Preference addShipment = shipmentGroup.findPreference("Shipment Add");
                    Criteria.checkThat(
                            "Add shipment button should NOT exist in category",
                            addShipment,
                            Matchers.nullValue());
                });
    }

    @Test
    @MediumTest
    public void testScreenSetup() {
        mSettingsActivityTestRule.startSettingsActivity();

        AutofillShoppingFragment fragment = mSettingsActivityTestRule.getFragment();
        assertThat(fragment.getPageTitle().get())
                .isEqualTo(
                        mSettingsActivityTestRule
                                .getActivity()
                                .getString(R.string.autofill_shopping_title));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThat(fragment.getPreferenceScreen().shouldUseGeneratedIds()).isFalse();
                });
    }

    @Test
    @MediumTest
    public void testAutofillAiEntities_opensEditorOnSuccessfulReauth() {
        EntityType orderType = TestUtils.getOrderEntityType();
        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        orderType,
                        /* entityInstanceLabel= */ "Order",
                        /* entityInstanceSubLabel= */ "Store",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(orderType, Arrays.asList(entity1));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(orderType)
                        .setGuid("guid1")
                        .setRecordType(RecordType.LOCAL)
                        .setUseCount(0)
                        .setRequiresReauthToSee(true)
                        .build();

        when(mEntityDataManager.getEntityInstance("guid1")).thenReturn(entityInstance);

        mSettingsActivityTestRule.startSettingsActivity();

        Preference orderEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));

        ThreadUtils.runOnUiThreadBlocking(orderEntity::performClick);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        ThreadUtils.runOnUiThreadBlocking(() -> callbackCaptor.getValue().onResult(true));

        onView(withText("Edit order")).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testToggle_correctStateWhenTurnedOff() {
        mSettingsActivityTestRule.startSettingsActivity();
        setShoppingTogglePreference(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSwitchPreference toggle =
                            mSettingsActivityTestRule
                                    .getFragment()
                                    .findPreference(AutofillShoppingFragment.PREF_OPT_IN_TOGGLE);
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
        setShoppingTogglePreference(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSwitchPreference toggle =
                            mSettingsActivityTestRule
                                    .getFragment()
                                    .findPreference(AutofillShoppingFragment.PREF_OPT_IN_TOGGLE);
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
                    AutofillShoppingFragment fragment = mSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference toggle =
                            fragment.findPreference(AutofillShoppingFragment.PREF_OPT_IN_TOGGLE);
                    assertNotNull(toggle);
                    assertThat(toggle.isVisible()).isTrue();
                    assertThat(toggle.isEnabled()).isFalse();
                    assertThat(toggle.isChecked()).isFalse();
                });
    }

    private void setShoppingTogglePreference(boolean value) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(mSettingsActivityTestRule.getFragment().getProfile())
                                .setBoolean(Pref.AUTOFILL_AI_SHOPPING_ENTITIES_ENABLED, value));
    }
}
