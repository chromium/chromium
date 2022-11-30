// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.nio.ByteBuffer;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Test relating to {@link StorePersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class StorePersistedTabDataTest {
    @Rule
    public JniMocker mMocker = new JniMocker();

    @Mock
    protected EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    protected Profile mProfileMock;

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

    private static final String MOCK_ENDPOINT_RESPONSE_STRING =
            "{\"representations\":[{\"type\":\"STORE_HOURS\",\"storeHo"
            + "urs\":[{\"label\":\"Hours\",\"storeHours\":{\"days\":{\"dayR"
            + "ange\":[{\"startDay\":\"MONDAY\",\"endDay\":\"SUNDAY\",\"low"
            + "Confidence\":false}]},\"isClosed\":false,\"lowConfiden"
            + "ce\":false,\"isOpen24Hours\":false,\"timeSpan\":[{\"open"
            + "ingTime\":{\"hour\":6,\"amPm\":\"AM\"},\"closingTime\":{\"ho"
            + "ur\":11,\"amPm\":\"PM\"}}]}},{\"label\":\"Pharmacy\",\"store"
            + "Hours\":{\"days\":{\"dayRange\":[{\"startDay\":\"MONDAY\",\""
            + "endDay\":\"FRIDAY\",\"lowConfidence\":false}]},\"isClose"
            + "d\":false,\"lowConfidence\":false,\"isOpen24Hours\":fal"
            + "se,\"timeSpan\":[{\"openingTime\":{\"hour\":10,\"amPm\":\"A"
            + "M\"},\"closingTime\":{\"hour\":6,\"minute\":30,\"amPm\":\"PM"
            + "\"}}]}},{\"label\":\"Closed for Lunch\",\"storeHours\":{\""
            + "days\":{\"day\":[\"SATURDAY\"]},\"isClosed\":false,\"lowCo"
            + "nfidence\":false,\"isOpen24Hours\":false,\"timeSpan\":["
            + "{\"openingTime\":{\"hour\":9,\"amPm\":\"AM\"},\"closingTime"
            + "\":{\"hour\":5,\"minute\":30,\"amPm\":\"PM\"}}]}},{\"label\":"
            + "\"Online Shopping\",\"storeHours\":{\"days\":{\"dayRange\""
            + ":[{\"startDay\":\"MONDAY\",\"endDay\":\"SUNDAY\",\"lowConfi"
            + "dence\":false}]},\"isClosed\":false,\"lowConfidence\":f"
            + "alse,\"isOpen24Hours\":false,\"timeSpan\":[{\"openingTi"
            + "me\":{\"hour\":9,\"amPm\":\"AM\"},\"closingTime\":{\"hour\":8"
            + ",\"amPm\":\"PM\"}}]}},{\"label\":\"Bakery\",\"storeHours\":{"
            + "\"days\":{\"dayRange\":[{\"startDay\":\"MONDAY\",\"endDay\":"
            + "\"SUNDAY\",\"lowConfidence\":false}]},\"isClosed\":false"
            + ",\"lowConfidence\":false,\"isOpen24Hours\":false,\"time"
            + "Span\":[{\"openingTime\":{\"hour\":6,\"amPm\":\"AM\"},\"clos"
            + "ingTime\":{\"hour\":6,\"amPm\":\"PM\"}}]}},{\"label\":\"Cafe"
            + "\",\"storeHours\":{\"days\":{\"dayRange\":[{\"startDay\":\"M"
            + "ONDAY\",\"endDay\":\"SUNDAY\",\"lowConfidence\":false}]},"
            + "\"isClosed\":false,\"lowConfidence\":false,\"isOpen24Ho"
            + "urs\":false,\"timeSpan\":[{\"openingTime\":{\"hour\":6,\"a"
            + "mPm\":\"AM\"},\"closingTime\":{\"hour\":8,\"amPm\":\"PM\"}}]}"
            + "},{\"label\":\"Deli\",\"storeHours\":{\"days\":{\"dayRange\""
            + ":[{\"startDay\":\"MONDAY\",\"endDay\":\"SUNDAY\",\"lowConfi"
            + "dence\":false}]},\"isClosed\":false,\"lowConfidence\":f"
            + "alse,\"isOpen24Hours\":false,\"timeSpan\":[{\"openingTi"
            + "me\":{\"hour\":6,\"amPm\":\"AM\"},\"closingTime\":{\"hour\":9"
            + ",\"amPm\":\"PM\"}}]}},{\"label\":\"Hot Wok\",\"storeHours\":"
            + "{\"days\":{\"dayRange\":[{\"startDay\":\"MONDAY\",\"endDay\""
            + ":\"SUNDAY\",\"lowConfidence\":false}]},\"isClosed\":fals"
            + "e,\"lowConfidence\":false,\"isOpen24Hours\":false,\"tim"
            + "eSpan\":[{\"openingTime\":{\"hour\":9,\"amPm\":\"AM\"},\"clo"
            + "singTime\":{\"hour\":6,\"amPm\":\"PM\"}}]}},{\"label\":\"Mea"
            + "t\",\"storeHours\":{\"days\":{\"dayRange\":[{\"startDay\":\""
            + "MONDAY\",\"endDay\":\"SUNDAY\",\"lowConfidence\":false}]}"
            + ",\"isClosed\":false,\"lowConfidence\":false,\"isOpen24H"
            + "ours\":false,\"timeSpan\":[{\"openingTime\":{\"hour\":6,\""
            + "amPm\":\"AM\"},\"closingTime\":{\"hour\":8,\"amPm\":\"PM\"}}]"
            + "}},{\"label\":\"Produce\",\"storeHours\":{\"days\":{\"dayRa"
            + "nge\":[{\"startDay\":\"MONDAY\",\"endDay\":\"SUNDAY\",\"lowC"
            + "onfidence\":false}]},\"isClosed\":false,\"lowConfidenc"
            + "e\":false,\"isOpen24Hours\":false,\"timeSpan\":[{\"openi"
            + "ngTime\":{\"hour\":6,\"amPm\":\"AM\"},\"closingTime\":{\"hou"
            + "r\":9,\"amPm\":\"PM\"}}]}}]}]}";

    private static final String EXPECTED_RESPONSE_GENERAL_CASE = "6:00 A.M - 11:00 P.M";

    private static final String EXPECTED_RESPONSE_EMPTY_CASE = "";

    private static final String EMPTY_ENDPOINT_RESPONSE = "";

    private static final String MALFORMED_ENDPOINT_RESPONSE = "malformed response";

    private static final int GET_TIME_STRING_INPUT_TIME = 514;

    private static final String GET_TIME_STRING_EXPECTED_RETURN = "5:14 A.M";

    private static final int GET_TIME_STRING_INPUT_TIME_AM_DOUBLE_DIGIT_CASE = 1130;

    private static final String GET_TIME_STRING_EXPECTED_RETURN_AM_DOUBLE_DIGIT_CASE = "11:30 A.M";

    private static final int GET_TIME_STRING_INPUT_TIME_AM_CASE = 23;

    private static final String GET_TIME_STRING_EXPECTED_RETURN_AM_CASE = "12:23 A.M";

    private static final int GET_TIME_STRING_INPUT_TIME_SINGLE_DIGIT_CASE = 3;

    private static final String GET_TIME_STRING_EXPECTED_RETURN_SINGLE_DIGIT_CASE = "12:03 A.M";

    private static final int GET_TIME_STRING_INPUT_TIME_PM_CASE = 1514;

    private static final String GET_TIME_STRING_EXPECTED_RETURN_PM_CASE = "3:14 P.M";

    private static final int GET_TIME_STRING_INPUT_TIME_PM_DOUBLE_DIGITS_CASE = 2354;

    private static final String GET_TIME_STRING_EXPECTED_RETURN_PM_DOUBLE_DIGITS_CASE = "11:54 P.M";

    private static final int GET_TIME_STRING_INPUT_TIME_BAD_CASE = -1;

    private static final String GET_TIME_STRING_EXPECTED_RETURN_BAD_CASE = "";

    private static final int GET_TIME_STRING_INPUT_TIME_IS_PM_CASE = 514;

    private static final String GET_TIME_STRING_EXPECTED_RETURN_IS_PM_CASE = "5:14 P.M";

    private static final int SERIALIZE_DESERIALIZE_OPENING_TIME = 600;

    private static final int SERIALIZE_DESERIALIZE_CLOSING_TIME = 2230;

    @SmallTest
    @Test
    public void testNormalResponse() throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MOCK_ENDPOINT_RESPONSE_STRING);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            StorePersistedTabData.from(tab, (res) -> {
                Assert.assertEquals(EXPECTED_RESPONSE_GENERAL_CASE, res.getStoreHoursString());
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testEmptyResponse() throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(EMPTY_ENDPOINT_RESPONSE);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            StorePersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testMalformedResponse() throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MALFORMED_ENDPOINT_RESPONSE);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            StorePersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutes() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME, false);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN, res);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutesAmDoubleDigit() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME_AM_DOUBLE_DIGIT_CASE, false);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN_AM_DOUBLE_DIGIT_CASE, res);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutesAm() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME_AM_CASE, false);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN_AM_CASE, res);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutesSingleDigit() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME_SINGLE_DIGIT_CASE, false);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN_SINGLE_DIGIT_CASE, res);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutesPm() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME_PM_CASE, true);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN_PM_CASE, res);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutesPmDoubleDigits() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME_PM_DOUBLE_DIGITS_CASE, true);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN_PM_DOUBLE_DIGITS_CASE, res);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutesBad() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME_BAD_CASE, true);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN_BAD_CASE, res);
    }

    @SmallTest
    @Test
    public void testGetTimeStringFromStandardTimeMinutesIsPm() {
        String res = StorePersistedTabData.getTimeStringFromStandardTimeMinutes(
                GET_TIME_STRING_INPUT_TIME_IS_PM_CASE, true);
        Assert.assertEquals(GET_TIME_STRING_EXPECTED_RETURN_IS_PM_CASE, res);
    }

    @SmallTest
    @Test
    public void testSerializeDeserialize() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        StorePersistedTabData storePersistedTabData = new StorePersistedTabData(tab,
                new StorePersistedTabData.StoreHours(
                        SERIALIZE_DESERIALIZE_OPENING_TIME, SERIALIZE_DESERIALIZE_CLOSING_TIME));
        ByteBuffer serialized = storePersistedTabData.getSerializer().get();
        StorePersistedTabData deserialized = new StorePersistedTabData(tab);
        deserialized.deserialize(serialized);
        Assert.assertEquals(
                SERIALIZE_DESERIALIZE_OPENING_TIME, deserialized.mStoreHours.mOpeningTime);
        Assert.assertEquals(
                SERIALIZE_DESERIALIZE_CLOSING_TIME, deserialized.mStoreHours.mClosingTime);
    }

    @SmallTest
    @Test
    public void testSerializeDeserializeNull() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        StorePersistedTabData deserialized = new StorePersistedTabData(tab, null);
        Assert.assertFalse(deserialized.deserialize(null));
    }

    private void mockEndpointResponse(String response) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[9];
                callback.onResult(new EndpointResponse(response));
                return null;
            }
        })
                .when(mEndpointFetcherJniMock)
                .nativeFetchOAuth(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), any(String[].class), anyString(), anyLong(), anyInt(),
                        any(Callback.class));
    }
}
