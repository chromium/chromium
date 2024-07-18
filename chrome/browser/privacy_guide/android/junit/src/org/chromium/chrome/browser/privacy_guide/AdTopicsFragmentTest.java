// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/** JUnit tests of the class {@link AdTopicsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdTopicsFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    private FragmentScenario mScenario;
    private MaterialSwitchWithText mAdTopicsButton;

    // TODO(b/353975355): Add more tests for Ad Topics step in privacy guide
    private void initFragmentWithDefaultState() {
        mScenario =
                FragmentScenario.launchInContainer(
                        AdTopicsFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof AdTopicsFragment) {
                                    ((AdTopicsFragment) fragment).setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment ->
                        mAdTopicsButton = fragment.getView().findViewById(R.id.ad_topics_switch));
    }

    @Test
    public void testIsSwitchOffByDefault() {
        initFragmentWithDefaultState();
        assertFalse(mAdTopicsButton.isChecked());
    }
}
