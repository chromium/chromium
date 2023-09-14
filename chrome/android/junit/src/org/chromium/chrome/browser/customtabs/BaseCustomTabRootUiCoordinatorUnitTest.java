// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.sync.SyncService;

/** JUnit tests for BaseCustomTabRootUiCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public final class BaseCustomTabRootUiCoordinatorUnitTest {
    @Rule
    public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    private void enablePageInsights(CustomTabsConnection connection, SyncService syncService) {
        when(connection.shouldEnablePageInsightsForIntent(any())).thenReturn(true);
        when(syncService.isSyncingUnencryptedUrls()).thenReturn(true);
    }

    private boolean isPageInsightsEnabledSync() {
        return BaseCustomTabRootUiCoordinator.isPageInsightsHubEnabledSync(null, () -> null);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB)
    public void testPageInsightsEnabledSync_cctPageInsightsHubTrue() throws Exception {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        SyncService syncService = Mockito.mock(SyncService.class);
        SyncServiceFactory.setInstanceForTesting(syncService);

        enablePageInsights(connection, syncService);
        assertTrue("PageInsightsHub should be enabled", isPageInsightsEnabledSync());

        // The method should return false if any one of the conditions is not met .

        enablePageInsights(connection, syncService);
        when(connection.shouldEnablePageInsightsForIntent(any())).thenReturn(false);
        assertFalse("PageInsightsHub should be disabled", isPageInsightsEnabledSync());

        enablePageInsights(connection, syncService);
        when(syncService.isSyncingUnencryptedUrls()).thenReturn(false);
        assertFalse("PageInsightsHub should be disabled", isPageInsightsEnabledSync());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB)
    public void testPageInsightsEnabledSync_cctPageInsightsHubFalse() throws Exception {
        CustomTabsConnection connection = Mockito.mock(CustomTabsConnection.class);
        CustomTabsConnection.setInstanceForTesting(connection);

        SyncService syncService = Mockito.mock(SyncService.class);
        SyncServiceFactory.setInstanceForTesting(syncService);

        // The method returns false if the flag is set to false

        enablePageInsights(connection, syncService);
        assertFalse("PageInsightsHub should be disabled", isPageInsightsEnabledSync());
    }
}
