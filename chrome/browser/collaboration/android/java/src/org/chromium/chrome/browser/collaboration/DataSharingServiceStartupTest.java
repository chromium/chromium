// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingSDKDelegateBridge;
import org.chromium.components.data_sharing.DataSharingServiceImpl;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests the startup behavior of the DataSharingService to ensure that heavy SDK components are not
 * loaded for users without any shared groups.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
@Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_20W02)
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/419289558): Re-enable color surface feature flags.
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE,
    ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID
})
public class DataSharingServiceStartupTest {
    // Reset to a single blank tab between each test, without restarting the ChromeTabbedActivity.
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private TabGroupSyncService.Observer mObserver;

    @Before
    public void setUp() {
        mSigninTestRule.addAccountThenSigninAndEnableHistorySync(TestAccounts.ACCOUNT1);
    }

    @After
    public void tearDown() {
        // Clean up the observer to avoid leaks.
        if (mObserver == null) return;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupSyncService tabGroupSyncService =
                            TabGroupSyncServiceFactory.getForProfile(
                                    mCtaTestRule.getProfile(false));
                    if (tabGroupSyncService != null) {
                        tabGroupSyncService.removeObserver(mObserver);
                    }
                });
    }

    @Test
    @SmallTest
    public void testShareKitSdkNotLoadedForZeroGroups() throws TimeoutException {
        // TODO(ritikagup) : A more direct approach would be to wait for
        // DataSharingService's OnGroupDataModelLoaded() event, but this is not
        // currently exposed in Java.
        triggerAndAwaitTabGroupSyncInitialization();

        // With a clean profile and after services are initialized, the SDK bridge
        // should not have been created because there are no shared groups.
        Assert.assertFalse(
                "DataSharingSDKDelegateBridge should not be initialized for zero groups.",
                DataSharingSDKDelegateBridge.isInitializedForTesting());

        // TODO(ritikagup): Verify that the DataSharingUIDelegate has not been created either.
    }

    @Test
    @SmallTest
    public void testShareKitSdkNotLoadedForOneTabGroup() throws TimeoutException {
        // Creating a tab group.
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabs(
                        firstPage,
                        /* numRegularTabs= */ 2,
                        /* numIncognitoTabs= */ 0,
                        /* url= */ "about:blank",
                        WebPageStation::newBuilder);
        RegularTabSwitcherStation tabSwitcherStation = pageStation.openRegularTabSwitcher();
        Journeys.mergeAllTabsToNewGroup(tabSwitcherStation);

        // TODO(ritikagup) : A more direct approach would be to wait for DataSharingService's
        // OnGroupDataModelLoaded() event, but this is notcurrently exposed in Java.
        triggerAndAwaitTabGroupSyncInitialization();

        // The SDK should not be loaded for a regular tab group.
        Assert.assertFalse(
                "DataSharingSDKDelegateBridge should not be initialized for one tab group.",
                DataSharingSDKDelegateBridge.isInitializedForTesting());

        // TODO(ritikagup): Verify that the DataSharingUIDelegate has not been created either.
    }

    /**
     * Attaches an observer and waits for the TabGroupSyncService to finish its asynchronous
     * initialization. This is used as a signal that dependent services have likely initialized as
     * well.
     */
    private void triggerAndAwaitTabGroupSyncInitialization() throws TimeoutException {
        final CallbackHelper onTabGroupSyncInitialized = new CallbackHelper();
        mObserver =
                new TabGroupSyncService.Observer() {
                    @Override
                    public void onInitialized() {
                        onTabGroupSyncInitialized.notifyCalled();
                    }
                };

        // Get the call count before adding the observer on the UI thread.
        final int callCount = onTabGroupSyncInitialized.getCallCount();

        // Get services and attach observers on the UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = mCtaTestRule.getProfile(false);
                    DataSharingServiceImpl dataSharingService =
                            (DataSharingServiceImpl)
                                    DataSharingServiceFactory.getForProfile(profile);
                    Assert.assertFalse(dataSharingService.isEmptyService());

                    // Get CollaborationService to ensure it is created.
                    CollaborationService collaborationService =
                            CollaborationServiceFactory.getForProfile(profile);
                    Assert.assertNotNull(collaborationService);

                    TabGroupSyncService tabGroupSyncService =
                            TabGroupSyncServiceFactory.getForProfile(profile);
                    Assert.assertNotNull(tabGroupSyncService);
                    tabGroupSyncService.addObserver(mObserver);
                });

        // Wait for the onInitialized() callback.
        onTabGroupSyncInitialized.waitForCallback(callCount);
    }
}
