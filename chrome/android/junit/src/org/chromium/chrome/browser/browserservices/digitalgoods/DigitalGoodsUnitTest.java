// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Tests the flow from DigitalGoodsImpl, through DigitalGoodsAdapter and the converts to the
 * TrustedWebActivityClient and back again. It uses a mock TrustedWebActivityClient.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class DigitalGoodsUnitTest {
    private final DigitalGoodsImpl.Delegate mDelegate =
            () -> {
                GURL url = Mockito.mock(GURL.class);
                when(url.getSpec()).thenReturn("http://www.example.com/");
                return url;
            };
    private final MockTrustedWebActivityClient mClient = new MockTrustedWebActivityClient();

    private DigitalGoodsImpl mDigitalGoods;

    @Before
    public void setUp() {
        mDigitalGoods =
                new DigitalGoodsImpl(new DigitalGoodsAdapter(mClient.getClient()), mDelegate);
    }

    @Test
    public void getDetails() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        mDigitalGoods.getDetails(
                new String[] {"token 1", "token 2"},
                (code, details) -> {
                    assertEquals(2, details.length);

                    assertEquals("id1", details[0].itemId);
                    assertEquals("id2", details[1].itemId);

                    callbackHelper.notifyCalled();
                });
        mClient.runCallback();

        callbackHelper.waitForCallback(0);
    }

    @Test
    public void listPurchases() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        mDigitalGoods.listPurchases(
                (code, purchases) -> {
                    callbackHelper.notifyCalled();

                    assertEquals("id3", purchases[0].itemId);
                    assertEquals("id4", purchases[1].itemId);
                });
        mClient.runCallback();

        callbackHelper.waitForCallback(0);
    }

    @Test
    public void listPurchaseHistory() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        mDigitalGoods.listPurchaseHistory(
                (code, purchases) -> {
                    assertEquals("id4", purchases[0].itemId);
                    assertEquals("id3", purchases[1].itemId);

                    callbackHelper.notifyCalled();
                });
        mClient.runCallback();

        callbackHelper.waitForCallback(0);
    }

    @Test
    public void consume() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        mDigitalGoods.consume(
                "token",
                (code) -> {
                    assertEquals(BillingResponseCode.OK, (int) code);

                    callbackHelper.notifyCalled();
                });

        mClient.runCallback();

        callbackHelper.waitForCallback(0);
    }

    @Test
    public void consume_oldClient() throws TimeoutException {
        mClient.setVersion(1);

        CallbackHelper callbackHelper = new CallbackHelper();
        mDigitalGoods.consume(
                "token",
                (code) -> {
                    assertEquals(BillingResponseCode.OK, (int) code);

                    callbackHelper.notifyCalled();
                });

        mClient.runCallback();

        callbackHelper.waitForCallback(0);
    }
}
