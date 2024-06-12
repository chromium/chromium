// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfoCallback;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Tests for {@link MerchantTrustSignalsDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class MerchantTrustSignalsDataProviderTest {

    @Mock private GURL mMockDestinationGurl;

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock private Profile mMockProfile;
    @Mock private ShoppingService mMockShoppingService;

    private final MerchantInfo mFakeMerchantTrustSignals =
            new MerchantInfo(4.5f, 100, new GURL(""), true, 0.2f, false, false);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mMockProfile).isOffTheRecord();
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
    }

    @Test
    public void testGetDataForUrlNullMetadata() throws TimeoutException {
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockShoppingServiceResponse(null);
        instance.getDataForUrl(mMockProfile, mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);
        Assert.assertNull(callbackHelper.getMerchantTrustSignalsResult());
    }

    @Test
    public void testGetDataForUrlValid() throws TimeoutException {
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockShoppingServiceResponse(mFakeMerchantTrustSignals);
        instance.getDataForUrl(mMockProfile, mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        MerchantInfo result = callbackHelper.getMerchantTrustSignalsResult();
        Assert.assertNotNull(result);
        Assert.assertEquals(4.5f, result.starRating, 0.0f);
        Assert.assertEquals(100, result.countRating);
        Assert.assertEquals(new GURL(""), result.detailsPageUrl);
        Assert.assertEquals(true, result.hasReturnPolicy);
        Assert.assertEquals(0.2f, result.nonPersonalizedFamiliarityScore, 0.0f);
        Assert.assertEquals(false, result.containsSensitiveContent);
        Assert.assertEquals(false, result.proactiveMessageDisabled);
    }

    @Test
    public void testGetDataForNullProfile() throws TimeoutException {
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockShoppingServiceResponse(mFakeMerchantTrustSignals);
        instance.getDataForUrl(null, mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);
        Assert.assertNull(callbackHelper.getMerchantTrustSignalsResult());
    }

    @Test
    public void testGetDataForIncognitoProfile() throws TimeoutException {
        doReturn(true).when(mMockProfile).isOffTheRecord();
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockShoppingServiceResponse(mFakeMerchantTrustSignals);
        instance.getDataForUrl(mMockProfile, mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);
        Assert.assertNull(callbackHelper.getMerchantTrustSignalsResult());
    }

    @Test
    public void testIsValidMerchantTrustSignals() {
        MerchantTrustSignalsDataProvider instance = getDataProvider();
        Assert.assertTrue(instance.isValidMerchantTrustSignals(mFakeMerchantTrustSignals));
    }

    @Test
    public void testIsValidMerchantTrustSignals_EmptyDetailsPageUrl() {
        MerchantTrustSignalsDataProvider instance = getDataProvider();
        MerchantInfo trustSignals = new MerchantInfo(4.5f, 100, null, true, 0.2f, false, false);
        Assert.assertFalse(instance.isValidMerchantTrustSignals(trustSignals));
    }

    @Test
    public void testIsValidMerchantTrustSignals_ContainsSensitiveContent() {
        MerchantTrustSignalsDataProvider instance = getDataProvider();
        MerchantInfo trustSignals =
                new MerchantInfo(4.5f, 100, new GURL(""), true, 0.2f, true, false);
        Assert.assertFalse(instance.isValidMerchantTrustSignals(trustSignals));
    }

    @Test
    public void testIsValidMerchantTrustSignals_NoRating() {
        MerchantTrustSignalsDataProvider instance = getDataProvider();
        MerchantInfo trustSignals =
                new MerchantInfo(0f, 100, new GURL(""), true, 0.2f, false, false);
        Assert.assertTrue(instance.isValidMerchantTrustSignals(trustSignals));
    }

    @Test
    public void testIsValidMerchantTrustSignals_NoReturnPolicy() {
        MerchantTrustSignalsDataProvider instance = getDataProvider();
        MerchantInfo trustSignals =
                new MerchantInfo(4.5f, 100, new GURL(""), false, 0.2f, false, false);
        Assert.assertTrue(instance.isValidMerchantTrustSignals(trustSignals));
    }

    @Test
    public void testIsValidMerchantTrustSignals_NoRatingOrReturnPolicy() {
        MerchantTrustSignalsDataProvider instance = getDataProvider();
        MerchantInfo trustSignals =
                new MerchantInfo(0f, 100, new GURL(""), false, 0.2f, false, false);
        Assert.assertFalse(instance.isValidMerchantTrustSignals(trustSignals));
    }

    private void mockShoppingServiceResponse(MerchantInfo merchantInfo) {
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    GURL url = (GURL) invocation.getArguments()[0];
                                    MerchantInfoCallback callback =
                                            (MerchantInfoCallback) invocation.getArguments()[1];
                                    callback.onResult(url, merchantInfo);
                                    return null;
                                })
                .when(mMockShoppingService)
                .getMerchantInfoForUrl(any(GURL.class), any(MerchantInfoCallback.class));
    }

    private MerchantTrustSignalsDataProvider getDataProvider() {
        return new MerchantTrustSignalsDataProvider();
    }
}
