// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.android.httpclient.SimpleHttpClient.HttpResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;

/** Unit test for {@link SimpleHttpClient}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SimpleHttpClientUnitTest {
    private static final long FAKE_NATIVE_POINTER = 123456789L;

    @Rule public JniMocker mMocker = new JniMocker();
    @Rule public MockitoRule mRule = MockitoJUnit.rule();

    @Mock public SimpleHttpClient.Natives mNativeMock;
    @Mock public Profile mMockProfile;

    @Captor public ArgumentCaptor<String[]> mHeaderKeysCaptor;
    @Captor public ArgumentCaptor<String[]> mHeaderValuesCaptor;

    private SimpleHttpClient mHttpClient;

    @Before
    public void setUp() {
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mMocker.mock(SimpleHttpClientJni.TEST_HOOKS, mNativeMock);
        Mockito.when(mNativeMock.init(mMockProfile)).thenReturn(FAKE_NATIVE_POINTER);

        mHttpClient = new SimpleHttpClient(mMockProfile);

        Mockito.verify(mNativeMock).init(mMockProfile);
    }

    @Test
    public void testDestroy() {
        mHttpClient.destroy();
        Mockito.verify(mNativeMock).destroy(FAKE_NATIVE_POINTER);
    }

    @Test
    public void testParseHeader() {
        final String keyFoo = "Foo";
        final String keyBar = "Bar";
        final String valFoo = "valFoo";
        final String valBar = "valBar";

        final Map<String, String> headers = new HashMap<>();
        headers.put(keyFoo, valFoo);
        headers.put(keyBar, valBar);

        final CallbackHelper responseCallback = new CallbackHelper();
        final byte[] requestBody = {};
        final String requestType = "requestType";

        mHttpClient.send(
                JUnitTestGURLs.BLUE_1,
                requestType,
                requestBody,
                headers,
                NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS,
                (response) -> responseCallback.notifyCalled());

        Mockito.verify(mNativeMock)
                .sendNetworkRequest(
                        eq(FAKE_NATIVE_POINTER),
                        eq(JUnitTestGURLs.BLUE_1),
                        eq(requestType),
                        eq(requestBody),
                        mHeaderKeysCaptor.capture(),
                        mHeaderValuesCaptor.capture(),
                        eq(NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS.getHashCode()),
                        any());

        // Verify the key value for the string does match.
        String[] headerKeys = mHeaderKeysCaptor.getValue();
        String[] headerValues = mHeaderValuesCaptor.getValue();

        Assert.assertEquals("Header key array length is different.", 2, headerKeys.length);
        Assert.assertEquals("Header value array length is different.", 2, headerValues.length);

        Assert.assertEquals(
                "Header keys and values should match at the index 0.",
                "val" + headerKeys[0],
                headerValues[0]);
        Assert.assertEquals(
                "Header keys and values should match at the index 1.",
                "val" + headerKeys[1],
                headerValues[1]);
    }

    @Test
    public void testCreateHttpResponse() {
        String[] responseHeaderKeys = {"Foo", "Foo", "Bar"};
        String[] responseHeaderValues = {"valFoo1", "valFoo2", "valBar"};
        byte[] responseBody = "responseBody".getBytes();

        HttpResponse response =
                SimpleHttpClient.createHttpResponse(
                        200, 0, responseBody, responseHeaderKeys, responseHeaderValues);

        Assert.assertNotNull(response.mHeaders.get("Foo"));
        Assert.assertNotNull(response.mHeaders.get("Bar"));

        Assert.assertEquals(200, response.mResponseCode);
        Assert.assertEquals(responseBody, response.mBody);
        Assert.assertEquals("valFoo1\nvalFoo2", response.mHeaders.get("Foo"));
        Assert.assertEquals("valBar", response.mHeaders.get("Bar"));
    }
}
