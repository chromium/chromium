// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertNotNull;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.creator.test.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.TestActivity;

/**
 * Tests for {@link CreatorCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorCoordinatorTest {
    private TestActivity mActivity;
    private CreatorCoordinator mCreatorCoordinator;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    private Profile mProfile;

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);

        mCreatorCoordinator = new CreatorCoordinator(mActivity);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testCreatorCoordinatorConstruction() {
        assertNotNull("Could not construct CreatorCoordinator", mCreatorCoordinator);
    }

    @Test
    public void testActionBar() {
        View outerView = mCreatorCoordinator.getView();
        ViewGroup actionBar = (ViewGroup) outerView.findViewById(R.id.action_bar);
        assertNotNull("Could not retrieve ActionBar", actionBar);
    }
}
