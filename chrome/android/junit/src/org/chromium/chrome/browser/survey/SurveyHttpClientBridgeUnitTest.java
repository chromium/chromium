// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import org.junit.After;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.survey.SurveyHttpClientBridge.HttpResponse;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Unit test for {@link SurveyHttpClientBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Mocks GURL
public class SurveyHttpClientBridgeUnitTest {
    private static final long FAKE_NATIVE_POINTER = 123456789L;

    @Rule
    public JniMocker mMocker = new JniMocker();
    @Rule
    public MockitoRule mRule = MockitoJUnit.rule();

    @Mock
    public SurveyHttpClientBridge.Natives mNativeMock;
    @Mock
    public Profile mMockProfile;
    @Mock
    public GURL mMockGurl;

    @Captor
    public ArgumentCaptor<String[]> mHeaderKeysCaptor;
    @Captor
    public ArgumentCaptor<String[]> mHeaderValuesCaptor;

    private SurveyHttpClientBridge mSurveyHttpClientBridge;

    @Before
    public void setUp() {
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mMocker.mock(SurveyHttpClientBridgeJni.TEST_HOOKS, mNativeMock);
        Mockito.when(mNativeMock.init(HttpClientType.SURVEY, mMockProfile))
                .thenReturn(FAKE_NATIVE_POINTER);

        mSurveyHttpClientBridge = new SurveyHttpClientBridge(HttpClientType.SURVEY, mMockProfile);

        Mockito.verify(mNativeMock).init(HttpClientType.SURVEY, mMockProfile);
    }

    @After
    public void tearDown() {
        ThreadUtils.setThreadAssertsDisabledForTesting(false);
    }

    @Test
    public void testDestroy() {
        mSurveyHttpClientBridge.destroy();
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

        Mockito.when(mMockGurl.isValid()).thenReturn(true);
        mSurveyHttpClientBridge.send(mMockGurl, requestType, requestBody, headers,
                (response) -> responseCallback.notifyCalled());

        Mockito.verify(mNativeMock)
                .sendNetworkRequest(eq(FAKE_NATIVE_POINTER), eq(mMockGurl), eq(requestType),
                        eq(requestBody), mHeaderKeysCaptor.capture(), mHeaderValuesCaptor.capture(),
                        any());

        // Verify the key value for the string does match.
        String[] headerKeys = mHeaderKeysCaptor.getValue();
        String[] headerValues = mHeaderValuesCaptor.getValue();

        Assert.assertEquals("Header key array length is different.", 2, headerKeys.length);
        Assert.assertEquals("Header value array length is different.", 2, headerValues.length);

        Assert.assertEquals("Header keys and values should match at the index 0.",
                "val" + headerKeys[0], headerValues[0]);
        Assert.assertEquals("Header keys and values should match at the index 1.",
                "val" + headerKeys[1], headerValues[1]);
    }

    @Test
    public void testCreateHttpResponse() {
        String[] responseHeaderKeys = {"Foo", "Foo", "Bar"};
        String[] responseHeaderValues = {"valFoo1", "valFoo2", "valBar"};
        byte[] responseBody = "responseBody".getBytes();

        HttpResponse response = SurveyHttpClientBridge.createHttpResponse(
                200, 0, responseBody, responseHeaderKeys, responseHeaderValues);

        Assert.assertNotNull(response.mHeaders.get("Foo"));
        Assert.assertNotNull(response.mHeaders.get("Bar"));

        Assert.assertEquals(2, response.mHeaders.get("Foo").size());
        Assert.assertEquals(1, response.mHeaders.get("Bar").size());
        Assert.assertEquals(200, response.mResponseCode);
        Assert.assertEquals(responseBody, response.mBody);
    }
}
