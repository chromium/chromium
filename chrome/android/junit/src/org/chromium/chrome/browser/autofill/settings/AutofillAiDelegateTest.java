// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.os.Bundle;
import android.widget.ImageView;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.preference.Preference;

import org.jspecify.annotations.NonNull;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;

/** Unit tests for {@link AutofillAiDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT,
    ChromeFeatureList.AUTOFILL_AI_SHOW_WALLET_DISABLED_BANNER,
    ChromeFeatureList.AUTOFILL_ENABLE_WALLET_DISCLOSURE_NOTICE_PUBLIC_PASS,
})
public class AutofillAiDelegateTest {
    private static final String ENTITY_GUID = "entity-guid-1";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FragmentScenario<AutofillIdentityDocsFragment> mScenario;
    private AutofillIdentityDocsFragment mFragment;

    @Mock private EntityDataManager mEntityDataManager;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJniMock;

    @Before
    public void setUp() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    private void launchFragment() {
        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillIdentityDocsFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_Chromium_Settings,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof AutofillIdentityDocsFragment) {
                                    ((AutofillIdentityDocsFragment) fragment).setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(fragment -> mFragment = fragment);
        ShadowLooper.idleMainLooper();
    }

    @Test
    @DisableFeatures(AutofillFeatures.AUTOFILL_ENABLE_GRADIENT_GOOGLE_LOGOS)
    public void testWalletStoredEntityPreference_bindsGoogleWalletIcon() {
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                TestUtils.getPassportEntityType(),
                Collections.singletonList(
                        TestUtils.buildGermanyPassportWithLabels(
                                ENTITY_GUID,
                                /* storedInWallet= */ true,
                                /* walletEntityUrl= */ null)));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        launchFragment();

        Preference entityPref = mFragment.findPreference(ENTITY_GUID);
        assertNotNull(entityPref);
        assertEquals(R.layout.google_wallet_widget, entityPref.getWidgetLayoutResource());

        ImageView walletIcon = mFragment.getListView().findViewById(R.id.google_wallet_icon);
        assertNotNull(walletIcon);
        assertNotNull(walletIcon.getDrawable());
        assertEquals(
                R.drawable.google_wallet_24dp,
                shadowOf(walletIcon.getDrawable()).getCreatedFromResId());
    }

    @Test
    @EnableFeatures(AutofillFeatures.AUTOFILL_ENABLE_GRADIENT_GOOGLE_LOGOS)
    public void testWalletStoredEntityPreference_bindsGradientGoogleWalletIcon() {
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                TestUtils.getPassportEntityType(),
                Collections.singletonList(
                        TestUtils.buildGermanyPassportWithLabels(
                                ENTITY_GUID,
                                /* storedInWallet= */ true,
                                /* walletEntityUrl= */ null)));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        launchFragment();

        Preference entityPref = mFragment.findPreference(ENTITY_GUID);
        assertNotNull(entityPref);
        assertEquals(R.layout.google_wallet_widget, entityPref.getWidgetLayoutResource());

        ImageView walletIcon = mFragment.getListView().findViewById(R.id.google_wallet_icon);
        assertNotNull(walletIcon);
        assertNotNull(walletIcon.getDrawable());
        assertEquals(
                R.drawable.googlewallet_icon_with_gradient,
                shadowOf(walletIcon.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testLocalEntityPreference_doesNotShowGoogleWalletIcon() {
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(
                TestUtils.getPassportEntityType(),
                Collections.singletonList(
                        TestUtils.buildGermanyPassportWithLabels(
                                ENTITY_GUID,
                                /* storedInWallet= */ false,
                                /* walletEntityUrl= */ null)));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        launchFragment();

        Preference entityPref = mFragment.findPreference(ENTITY_GUID);
        assertNotNull(entityPref);
        assertEquals(0, entityPref.getWidgetLayoutResource());
        assertNull(mFragment.getListView().findViewById(R.id.google_wallet_icon));
    }
}
