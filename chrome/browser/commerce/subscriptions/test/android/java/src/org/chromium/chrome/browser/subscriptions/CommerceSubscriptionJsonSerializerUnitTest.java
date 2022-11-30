// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.PriceTrackableOffer;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests for {@link CommerceSubscriptionJsonSerializer}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommerceSubscriptionJsonSerializerUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String FAKE_OFFER_ID = "100";
    private static final String FAKE_PRODUCT_CLUSTER_ID = "300";
    private static final String FAKE_CURRENT_PRICE = "1000";
    private static final String FAKE_COUNTRY_CODE = "us";

    private static final String FAKE_SUBSCRIPTION_JSON_STRING = "{ \"type\": \"PRICE_TRACK\","
            + "\"managementType\": \"CHROME_MANAGED\", "
            + "\"identifierType\": \"OFFER_ID\", \"identifier\": \"100\","
            + "\"eventTimestampMicros\": \"200\" }";

    private static final CommerceSubscription FAKE_CHROME_MANAGED_SUBSCRIPTION =
            new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                    FAKE_OFFER_ID, CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                    CommerceSubscription.TrackingIdType.OFFER_ID, 200L);

    private static final CommerceSubscription FAKE_USER_MANAGED_SUBSCRIPTION =
            new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                    FAKE_PRODUCT_CLUSTER_ID,
                    CommerceSubscription.SubscriptionManagementType.USER_MANAGED,
                    CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID,
                    new PriceTrackableOffer(FAKE_OFFER_ID, FAKE_CURRENT_PRICE, FAKE_COUNTRY_CODE));

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @After
    public void tearDown() {
        DeferredStartupHandler.setInstanceForTests(null);
    }

    @Test
    public void testSerialize() throws JSONException {
        JSONObject subscriptionJson =
                CommerceSubscriptionJsonSerializer.serialize(FAKE_CHROME_MANAGED_SUBSCRIPTION);
        assertThat(subscriptionJson.getString("type"),
                equalTo(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK));
        assertThat(subscriptionJson.getString("identifierType"),
                equalTo(CommerceSubscription.TrackingIdType.OFFER_ID));
        assertThat(subscriptionJson.getString("identifier"), equalTo(FAKE_OFFER_ID));
    }

    @Test
    public void testSerialize_ClusterId() throws JSONException {
        JSONObject subscriptionJson =
                CommerceSubscriptionJsonSerializer.serialize(FAKE_USER_MANAGED_SUBSCRIPTION);
        assertThat(subscriptionJson.getString("type"),
                equalTo(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK));
        assertThat(subscriptionJson.getString("identifierType"),
                equalTo(CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID));
        assertThat(subscriptionJson.getString("identifier"), equalTo(FAKE_PRODUCT_CLUSTER_ID));
        assertThat(subscriptionJson.getJSONObject("userSeenOffer").getString("offerId"),
                equalTo(FAKE_OFFER_ID));
        assertThat(subscriptionJson.getJSONObject("userSeenOffer").getString("seenPriceMicros"),
                equalTo(FAKE_CURRENT_PRICE));
        assertThat(subscriptionJson.getJSONObject("userSeenOffer").getString("countryCode"),
                equalTo(FAKE_COUNTRY_CODE));
    }

    @Test
    public void testDeserialize() throws JSONException {
        JSONObject fakeSubscription = new JSONObject(FAKE_SUBSCRIPTION_JSON_STRING);
        CommerceSubscription actual =
                CommerceSubscriptionJsonSerializer.deserialize(fakeSubscription);

        assertThat(actual.getType(), equalTo(FAKE_CHROME_MANAGED_SUBSCRIPTION.getType()));
        assertThat(actual.getTimestamp(), equalTo(FAKE_CHROME_MANAGED_SUBSCRIPTION.getTimestamp()));
        assertThat(
                actual.getTrackingId(), equalTo(FAKE_CHROME_MANAGED_SUBSCRIPTION.getTrackingId()));
        assertThat(actual.getTrackingIdType(),
                equalTo(FAKE_CHROME_MANAGED_SUBSCRIPTION.getTrackingIdType()));
        assertThat(actual.getManagementType(),
                equalTo(FAKE_CHROME_MANAGED_SUBSCRIPTION.getManagementType()));
    }

    @Test
    public void testDeserialize_MissingTimestamp() throws JSONException {
        JSONObject fakeSubscription =
                new JSONObject("{ \"type\": \"PRICE_TRACK\", \"managementType\": "
                        + "\"CHROME_MANAGED\", \"identifierType\": \"OFFER_ID\", "
                        + "\"identifier\": \"100\" }");
        assertNull(CommerceSubscriptionJsonSerializer.deserialize(fakeSubscription));
    }

    @Test
    public void testDeserialize_MissingIdentifierOrType() throws JSONException {
        JSONObject fakeSubscriptionMissingId =
                new JSONObject("{ \"type\": \"PRICE_TRACK\", \"managementType\": "
                        + "\"CHROME_MANAGED\", \"identifierType\": \"OFFER_ID\", "
                        + "\"eventTimestampMicros\": \"200\" }");
        assertNull(CommerceSubscriptionJsonSerializer.deserialize(fakeSubscriptionMissingId));

        JSONObject fakeSubscriptionMissingIdType =
                new JSONObject("{ \"type\": \"PRICE_TRACK\", \"managementType\": "
                        + " \"CHROME_MANAGED\", \"identifier\": \"100\", "
                        + "\"eventTimestampMicros\": \"200\" }");
        assertNull(CommerceSubscriptionJsonSerializer.deserialize(fakeSubscriptionMissingIdType));
    }

    @Test
    public void testDeserialize_MissingManagementType() throws JSONException {
        JSONObject fakeSubscriptionMissingManagementType =
                new JSONObject("{ \"type\": \"PRICE_TRACK\", \"identifierType\": \"OFFER_ID\","
                        + " \"identifier\": \"100\", \"eventTimestampMicros\": \"200\" }");
        assertNull(CommerceSubscriptionJsonSerializer.deserialize(
                fakeSubscriptionMissingManagementType));
    }

    @Test
    public void testDeserialize_MissingType() throws JSONException {
        JSONObject fakeSubscriptionMissingManagementType =
                new JSONObject("{ \"managementType\": \"CHROME_MANAGED\", \"identifierType\":"
                        + "\"OFFER_ID\", \"identifier\": \"100\","
                        + " \"eventTimestampMicros\": \"200\" }");
        assertNull(CommerceSubscriptionJsonSerializer.deserialize(
                fakeSubscriptionMissingManagementType));
    }

    @Test
    public void testDeserialize_InvalidTimestamp() throws JSONException {
        JSONObject fakeSubscriptionInvalidTimestamp =
                new JSONObject("{ \"managementType\": \"CHROME_MANAGED\", "
                        + "\"identifierType\": \"OFFER_ID\", \"identifier\": \"100\", "
                        + "\"eventTimestampMicros\": \"lkjasdf\" }");
        assertNull(
                CommerceSubscriptionJsonSerializer.deserialize(fakeSubscriptionInvalidTimestamp));
    }

    @Test
    public void testDeserialize_ProductClusterId() throws JSONException {
        JSONObject subscriptionJson =
                new JSONObject("{ \"type\": \"PRICE_TRACK\", \"managementType\": \"USER_MANAGED\", "
                        + "\"identifierType\": \"PRODUCT_CLUSTER_ID\", \"identifier\": \"100\", "
                        + "\"eventTimestampMicros\": \"200\" }");
        CommerceSubscription actual =
                CommerceSubscriptionJsonSerializer.deserialize(subscriptionJson);
        assertThat(actual.getTrackingIdType(),
                equalTo(CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID));
        assertThat(actual.getManagementType(),
                equalTo(CommerceSubscription.SubscriptionManagementType.USER_MANAGED));
    }
}
