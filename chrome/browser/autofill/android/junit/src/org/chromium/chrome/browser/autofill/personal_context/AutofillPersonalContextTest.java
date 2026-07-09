// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.personal_context;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.net.Uri;

import androidx.fragment.app.testing.FragmentScenario;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Unit tests for {@link AutofillPersonalContextFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
    ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
})
public class AutofillPersonalContextTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FragmentScenario<AutofillPersonalContextFragment> mScenario;

    @Mock private EntityDataManager.Natives mMockEntityDataManagerJni;
    @Mock private Profile mProfile;
    @Mock private SettingsIndexData mSearchIndexDataMock;

    private AutofillPersonalContextFragment mFragment;
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActionTester = new UserActionTester();
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        EntityDataManagerJni.setInstanceForTesting(mMockEntityDataManagerJni);
        ProfileManager.setLastUsedProfileForTesting(mProfile);

        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillPersonalContextFragment.class,
                        null,
                        R.style.Theme_BrowserUI_DayNight);
        mScenario.onFragment(fragment -> mFragment = fragment);
    }

    @Test
    @SmallTest
    public void testPersonalContextSwitchToggleOff() {
        when(mMockEntityDataManagerJni.isPersonalContextEnabled(0L)).thenReturn(true);

        AutofillPersonalContextCoordinator.createFor(
                mFragment, mFragment.requireActivity(), mProfile);

        assertTrue(mFragment.getAutofillPersonalContextSwitch().isChecked());

        mFragment
                .getAutofillPersonalContextSwitch()
                .getOnPreferenceChangeListener()
                .onPreferenceChange(mFragment.getAutofillPersonalContextSwitch(), false);

        verify(mMockEntityDataManagerJni).setPersonalContextEnabled(0L, false);
        assertTrue(
                mActionTester
                        .getActions()
                        .contains(AutofillPersonalContextFragment.ACTION_TOGGLED_OFF));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testPersonalContextSwitchToggleOn() {
        when(mMockEntityDataManagerJni.isPersonalContextEnabled(0L)).thenReturn(false);

        AutofillPersonalContextCoordinator.createFor(
                mFragment, mFragment.requireActivity(), mProfile);

        assertFalse(mFragment.getAutofillPersonalContextSwitch().isChecked());

        mFragment
                .getAutofillPersonalContextSwitch()
                .getOnPreferenceChangeListener()
                .onPreferenceChange(mFragment.getAutofillPersonalContextSwitch(), true);

        verify(mMockEntityDataManagerJni).setPersonalContextEnabled(0L, true);
        assertTrue(
                mActionTester
                        .getActions()
                        .contains(AutofillPersonalContextFragment.ACTION_TOGGLED_ON));
    }

    @Test
    @SmallTest
    public void testPersonalContextManageConnectedAppsClick() {
        final String testUrl = "https://test.com/apps";
        when(mMockEntityDataManagerJni.getPersonalContextManageConnectedAppsUrl())
                .thenReturn(testUrl);

        AutofillPersonalContextCoordinator.createFor(
                mFragment, mFragment.requireActivity(), mProfile);

        mFragment
                .getAutofillPersonalContextManageConnectedApps()
                .getOnPreferenceClickListener()
                .onPreferenceClick(mFragment.getAutofillPersonalContextManageConnectedApps());

        Intent intent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(Uri.parse(testUrl), intent.getData());
        assertTrue(
                mActionTester
                        .getActions()
                        .contains(AutofillPersonalContextFragment.ACTION_MANAGE_CONNECTED_APPS));
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSearchIndexWhenFeaturesDisabled() {
        AutofillPersonalContextFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                RuntimeEnvironment.getApplication(), mSearchIndexDataMock, mProfile);

        verify(mSearchIndexDataMock)
                .removeEntry(
                        AutofillPersonalContextFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                AutofillPersonalContextFragment
                                        .PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        AutofillPersonalContextFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                AutofillPersonalContextFragment
                                        .PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS));
    }

    @Test
    @SmallTest
    public void testSearchIndexWhenCategoryNotVisible() {
        when(mMockEntityDataManagerJni.isPersonalContextPreferenceVisible(0L)).thenReturn(false);

        AutofillPersonalContextFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                RuntimeEnvironment.getApplication(), mSearchIndexDataMock, mProfile);

        verify(mSearchIndexDataMock)
                .removeEntry(
                        AutofillPersonalContextFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                AutofillPersonalContextFragment
                                        .PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        AutofillPersonalContextFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                AutofillPersonalContextFragment
                                        .PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS));
    }
}
