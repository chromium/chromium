// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

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
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.url.JUnitTestGURLs;

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
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
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
        final Map<String, String> headers = Map.of("Foo", "valFoo", "Bar", "valBar");
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
                        eq(headers),
                        eq(NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS.getHashCode()),
                        any());
    }
}
