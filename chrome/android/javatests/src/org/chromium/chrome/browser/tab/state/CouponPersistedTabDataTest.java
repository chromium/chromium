// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.nio.ByteBuffer;
import java.util.concurrent.ExecutionException;

/**
 * Test relating to {@link CouponPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CouponPersistedTabDataTest {
    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    private EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    private Profile mProfileMock;

    private static final String SERIALIZE_DESERIALIZE_NAME = "25% Off";
    private static final String SERIALIZE_DESERIALIZE_CODE = "DISCOUNT25";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersistedTabDataConfiguration.setUseTestConfig(true));
        Profile.setLastUsedProfileForTesting(mProfileMock);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
        PersistedTabDataConfiguration.setUseTestConfig(false);
    }

    @SmallTest
    @Test
    public void testSerializeDeserialize() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> new MockTab(1, false));
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab,
                new CouponPersistedTabData.Coupon(
                        SERIALIZE_DESERIALIZE_NAME, SERIALIZE_DESERIALIZE_CODE));
        ByteBuffer serialized = couponPersistedTabData.getSerializeSupplier().get();
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab);
        Assert.assertTrue(deserialized.deserialize(serialized));
        Assert.assertEquals(SERIALIZE_DESERIALIZE_NAME, deserialized.getCoupon().couponName);
        Assert.assertEquals(SERIALIZE_DESERIALIZE_CODE, deserialized.getCoupon().promoCode);
    }

    @SmallTest
    @Test
    public void testSerializeDeserializeNull() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> new MockTab(1, false));
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab, null);
        Assert.assertFalse(deserialized.deserialize(null));
    }

    @SmallTest
    @Test
    public void testSerializeDeserializeNoRemainingBytes() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> new MockTab(1, false));
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab, null);
        ByteBuffer serialized = couponPersistedTabData.getSerializeSupplier().get();
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab);
        Assert.assertFalse(serialized.hasRemaining());
        Assert.assertFalse(deserialized.deserialize(serialized));
    }
}
