// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link PriceTrackingUtilities}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class PriceTrackingUtilitiesTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private ProfileSyncService mProfileSyncServiceMock;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getIdentityManager(any(Profile.class)))
                .thenReturn(mIdentityManagerMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ProfileSyncService.overrideForTests(mProfileSyncServiceMock));

        setMbbStatus(true);
        setSignedInStatus(true);
        setTabSyncStatus(true, true);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
        IdentityServicesProvider.setInstanceForTests(null);
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(null);
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_tracking/true"})
    public void testIsPriceTrackingEligible() {
        Assert.assertTrue(PriceTrackingUtilities.isPriceTrackingEligible());
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_tracking/false"})
    public void testIsPriceTrackingEligibleFlagIsDisabled() {
        Assert.assertFalse(PriceTrackingUtilities.isPriceTrackingEligible());
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_tracking/true"})
    public void testIsPriceTrackingEligibleSyncDisabled() {
        setTabSyncStatus(false, false);
        Assert.assertFalse(PriceTrackingUtilities.isPriceTrackingEligible());
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_tracking/true"})

    public void testIsPriceTrackingEligibleNoSyncedTabs() {
        setTabSyncStatus(true, false);
        Assert.assertFalse(PriceTrackingUtilities.isPriceTrackingEligible());
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_tracking/true"})

    public void testIsPriceTrackingEligibleNoMbb() {
        setMbbStatus(false);
        Assert.assertFalse(PriceTrackingUtilities.isPriceTrackingEligible());
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_tracking/true"})

    public void testIsPriceTrackingEligibleNotSignedIn() {
        setSignedInStatus(false);
        Assert.assertFalse(PriceTrackingUtilities.isPriceTrackingEligible());
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_tracking/true"})
    public void testIsPriceTrackingEligibleTestHook() {
        setMbbStatus(false);
        setSignedInStatus(false);
        setTabSyncStatus(false, false);
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);

        Assert.assertTrue(PriceTrackingUtilities.isPriceTrackingEligible());
    }

    private void setMbbStatus(boolean isEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                Profile.getLastUsedRegularProfile(), isEnabled));
    }

    private void setSignedInStatus(boolean isSignedIn) {
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(isSignedIn);
    }

    private void setTabSyncStatus(boolean isSyncRequested, boolean hasSessions) {
        when(mProfileSyncServiceMock.isSyncRequested()).thenReturn(isSyncRequested);
        when(mProfileSyncServiceMock.getActiveDataTypes())
                .thenReturn(hasSessions ? CollectionUtil.newHashSet(ModelType.SESSIONS)
                                        : CollectionUtil.newHashSet(ModelType.AUTOFILL));
    }
}
