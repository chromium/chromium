// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubPasswordsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyHubPasswordsFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

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
        fragmentManager.beginTransaction().replace(android.R.id.content, mFragment).commitNow();
    }

    @Test
    public void verifyTitleAndEmptySkeletonPage() {
        launchFragment();

        assertEquals(
                mActivity.getString(R.string.password_list_title), mFragment.getPageTitle().get());
        assertNotNull(mFragment.getPreferenceScreen());
        assertEquals(0, mFragment.getPreferenceScreen().getPreferenceCount());
    }
}
