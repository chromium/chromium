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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;

import java.util.Set;

/** Tests for {@link PriceTrackingFeatures}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PriceTrackingFeaturesTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mActivityTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private SyncService mSyncServiceMock;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.start();

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getIdentityManager(any(Profile.class)))
                .thenReturn(mIdentityManagerMock);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);

        setMbbStatus(true);
        setSignedInStatus(true);
        setTabSyncStatus(true, true);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(true);
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligible() {
        Assert.assertTrue(
                PriceTrackingFeatures.isPriceAnnotationsEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleFlagIsDisabled() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        Assert.assertFalse(
                PriceTrackingFeatures.isPriceAnnotationsEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleNoMbb() {
        setMbbStatus(false);
        Assert.assertFalse(
                PriceTrackingFeatures.isPriceAnnotationsEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleNotSignedIn() {
        setSignedInStatus(false);
        Assert.assertFalse(
                PriceTrackingFeatures.isPriceAnnotationsEligible(
                        ProfileManager.getLastUsedRegularProfile()));
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testIsPriceTrackingEligibleIncognitoProfile() {
        OtrProfileId otrProfileId = OtrProfileId.createUnique("test:Incognito");
        Profile incognitoProfile =
                ProfileManager.getLastUsedRegularProfile()
                        .getOffTheRecordProfile(otrProfileId, /* createIfNeeded= */ true);
        Assert.assertFalse(PriceTrackingFeatures.isPriceAnnotationsEligible(incognitoProfile));
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
                PriceTrackingFeatures.isPriceAnnotationsEligible(
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
                .thenReturn(hasSessions ? Set.of(DataType.SESSIONS) : Set.of(DataType.AUTOFILL));
    }
}
