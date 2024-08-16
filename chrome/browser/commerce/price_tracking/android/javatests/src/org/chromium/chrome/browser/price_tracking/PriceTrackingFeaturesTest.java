// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;

/** Tests for {@link PriceTrackingFeatures}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PriceTrackingFeaturesTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private SyncService mSyncServiceMock;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getIdentityManager(any(Profile.class)))
                .thenReturn(mIdentityManagerMock);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);

        setMbbStatus(true);
        setSignedInStatus(true);
        setTabSyncStatus(true, true);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligible() {
        Assert.assertTrue(
                PriceTrackingFeatures.isPriceTrackingEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleFlagIsDisabled() {
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
        Assert.assertFalse(
                PriceTrackingFeatures.isPriceTrackingEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleNoMbb() {
        setMbbStatus(false);
        Assert.assertFalse(
                PriceTrackingFeatures.isPriceTrackingEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleNotSignedIn() {
        setSignedInStatus(false);
        Assert.assertFalse(
                PriceTrackingFeatures.isPriceTrackingEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleIncognitoProfile() {
        OTRProfileID otrProfileID = OTRProfileID.createUnique("test:Incognito");
        Profile incognitoProfile =
                ProfileManager.getLastUsedRegularProfile()
                        .getOffTheRecordProfile(otrProfileID, /* createIfNeeded= */ true);
        Assert.assertFalse(PriceTrackingFeatures.isPriceTrackingEligible(incognitoProfile));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleTestHook() {
        setMbbStatus(false);
        setSignedInStatus(false);
        setTabSyncStatus(false, false);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        Assert.assertTrue(
                PriceTrackingFeatures.isPriceTrackingEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    private void setMbbStatus(boolean isEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                ProfileManager.getLastUsedRegularProfile(), isEnabled));
    }

    private void setSignedInStatus(boolean isSignedIn) {
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(isSignedIn);
    }

    private void setTabSyncStatus(boolean isSyncFeatureEnabled, boolean hasSessions) {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(isSyncFeatureEnabled);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(
                        hasSessions
                                ? CollectionUtil.newHashSet(DataType.SESSIONS)
                                : CollectionUtil.newHashSet(DataType.AUTOFILL));
    }
}
