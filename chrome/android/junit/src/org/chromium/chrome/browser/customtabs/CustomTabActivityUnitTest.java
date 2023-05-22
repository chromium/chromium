// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.List;

/** JUnit tests for CustomTabActivity. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class CustomTabActivityUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final List<ActivityController> mActivityControllerList = new ArrayList<>();

    private CustomTabActivity mCustomTabActivity;

    @Before
    public void setUp() {
        ActivityController<CustomTabActivity> activityController =
                Robolectric.buildActivity(CustomTabActivity.class, new Intent()).create();
        mCustomTabActivity = activityController.get();
        mActivityControllerList.add(activityController);
    }

    @After
    public void tearDown() {
        for (ActivityController activityController : mActivityControllerList) {
            activityController.destroy();
        }
        CustomTabsConnection.setInstanceForTesting(null);
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivity;
    }

    @Test
    public void testPageInsightsHubEnabled() throws Exception {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, false);
        FeatureList.setTestValues(testValues);

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        assertFalse("PageInsightsHub should be disabled", getActivity().isPageInsightsHubEnabled());

        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        assertFalse("PageInsightsHub should be disabled", getActivity().isPageInsightsHubEnabled());

        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, false);
        when(connection.shouldEnablePageInsightsForIntent(any())).thenReturn(true);
        assertFalse("PageInsightsHub should be disabled", getActivity().isPageInsightsHubEnabled());

        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        assertTrue("PageInsightsHub should be enabled", getActivity().isPageInsightsHubEnabled());
    }
}
