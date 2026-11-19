// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.MenuItem;

import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceCategory;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.util.CustomTabLauncher;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubPasswordsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyHubPasswordsFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private CustomTabLauncher mCustomTabLauncherMock;

    private TestActivity mActivity;
    private SafetyHubPasswordsFragment mFragment;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
    }

    private void launchFragment() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        mFragment =
                (SafetyHubPasswordsFragment)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        SafetyHubPasswordsFragment.class.getClassLoader(),
                                        SafetyHubPasswordsFragment.class.getName());
        mFragment.setProfile(mSafetyHubTestRule.getProfile());
        mFragment.setCustomTabLauncher(mCustomTabLauncherMock);
        fragmentManager.beginTransaction().replace(android.R.id.content, mFragment).commitNow();
    }

    @Test
    public void verifyTitleHeaderAndSection() {
        launchFragment();

        assertEquals(
                mActivity.getString(R.string.password_list_title), mFragment.getPageTitle().get());

        TextMessagePreference headerPref =
                mFragment.findPreference(SafetyHubPasswordsFragment.PREF_HEADER);
        assertNotNull(headerPref);
        assertEquals(
                mActivity.getString(R.string.safety_hub_passwords_subpage_header),
                headerPref.getSummary().toString());

        PreferenceCategory sectionPref =
                mFragment.findPreference(SafetyHubPasswordsFragment.PREF_PASSWORDS_SECTION);
        assertNotNull(sectionPref);
        assertEquals(
                mActivity.getString(R.string.password_list_title),
                sectionPref.getTitle().toString());
    }

    @Test
    public void helpMenuOpensHelpCenter() {
        launchFragment();

        MenuItem item = mock(MenuItem.class);
        doReturn(R.id.menu_id_targeted_help).when(item).getItemId();
        assertTrue(mFragment.onOptionsItemSelected(item));

        verify(mCustomTabLauncherMock, times(1))
                .openUrlInCct(any(), eq(SafetyHubBaseFragment.HELP_CENTER_URL));
    }
}
