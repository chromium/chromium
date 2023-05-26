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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.sync.SyncService;

import java.util.ArrayList;
import java.util.List;

/** JUnit tests for CustomTabActivity. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class CustomTabActivityUnitTest {
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

    private void enablePageInsights(FeatureList.TestValues testValues,
            CustomTabsConnection connection, SyncService syncService) {
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        when(connection.shouldEnablePageInsightsForIntent(any())).thenReturn(true);
        when(syncService.isSyncingUnencryptedUrls()).thenReturn(true);
    }

    @Test
    public void testPageInsightsHubEnabled() throws Exception {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, false);
        FeatureList.setTestValues(testValues);

        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        SyncService syncService = Mockito.mock(SyncService.class);
        SyncService.overrideForTests(syncService);

        enablePageInsights(testValues, connection, syncService);
        assertTrue("PageInsightsHub should be enabled", getActivity().isPageInsightsHubEnabled());

        // The method should return false if any one of the conditions is not met.
        enablePageInsights(testValues, connection, syncService);
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, false);
        assertFalse("PageInsightsHub should be disabled", getActivity().isPageInsightsHubEnabled());

        enablePageInsights(testValues, connection, syncService);
        when(connection.shouldEnablePageInsightsForIntent(any())).thenReturn(false);
        assertFalse("PageInsightsHub should be disabled", getActivity().isPageInsightsHubEnabled());

        enablePageInsights(testValues, connection, syncService);
        when(syncService.isSyncingUnencryptedUrls()).thenReturn(false);
        assertFalse("PageInsightsHub should be disabled", getActivity().isPageInsightsHubEnabled());
    }
}
