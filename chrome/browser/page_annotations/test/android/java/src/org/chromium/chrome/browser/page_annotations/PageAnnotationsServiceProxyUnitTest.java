// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.page_annotations.PageAnnotation.PageAnnotationType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/**
 * Tests for {@link PageAnnotationsServiceProxyUnitTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.PAGE_ANNOTATIONS_SERVICE + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class PageAnnotationsServiceProxyUnitTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final String EXPECTED_CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String EXPECTED_METHOD = "GET";
    private static final GURL DUMMY_PAGE_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
    private static final String EMPTY_RESPONSE = "{}";
    private static final String ENDPOINT_RESPONSE_BUYABLE_PRODUCT =
            "{\"annotations\":[{\"type\":\"BUYABLE_PRODUCT\",\"buyableProduct\":"
            + "{\"title\":\"foo title\", \"offerId\":\"123\", \"imageUrl\":\"https://images.com?q=1234\","
            + "\"currentPrice\":{\"currencyCode\":\"USD\",\"amountMicros\":\"123456789012345\"},"
            + "\"referenceType\":\"MAIN_PRODUCT\"}}]}";

    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    private Profile mProfile;

    private PageAnnotationsServiceProxy mServiceProxy;

    private static final String DEFAULT_ENDPOINT = "https://memex-pa.googleapis.com/v1/annotations";
    private static final String ENDPOINT_OVERRIDE = "my-endpoint.com";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        doReturn(false).when(mProfile).isOffTheRecord();
        mServiceProxy = new PageAnnotationsServiceProxy(mProfile);
    }

    @Test
    @SmallTest
    public void testEmptyUrl() {
        mServiceProxy.fetchAnnotations(
                GURL.emptyGURL(), (result) -> { Assert.assertNull(result); });
    }

    @Test
    @SmallTest
    public void testNullUrl() {
        mServiceProxy.fetchAnnotations(null, (result) -> { Assert.assertNull(result); });
    }

    @Test
    @SmallTest
    public void testSingleUrlAnnotations() {
        mockEndpointResponse(ENDPOINT_RESPONSE_BUYABLE_PRODUCT);
        mServiceProxy.fetchAnnotations(DUMMY_PAGE_URL, (result) -> {
            Assert.assertNotNull(result);
            Assert.assertNotNull(result.getAnnotations());
            verifyAnnotations(result.getAnnotations());
        });
    }

    @Test
    @SmallTest
    public void testEmptyServiceResponse() {
        mockEndpointResponse(EMPTY_RESPONSE);
        mServiceProxy.fetchAnnotations(DUMMY_PAGE_URL,
                (result) -> { Assert.assertTrue(result.getAnnotations().isEmpty()); });
    }

    @Test
    @SmallTest
    public void testDefaultRequestUrl() {
        Assert.assertEquals(DEFAULT_ENDPOINT,
                PageAnnotationsServiceConfig.PAGE_ANNOTATIONS_BASE_URL.getValue());
    }

    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:page_annotations_base_url/my-endpoint.com"})
    public void testRequestUrlOverride() {
        Assert.assertEquals(ENDPOINT_OVERRIDE,
                PageAnnotationsServiceConfig.PAGE_ANNOTATIONS_BASE_URL.getValue());
    }

    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:page_annotations_base_url/my-endpoint.com"})
    public void testFetchSinglePageAnnotationsUrl() {
        mServiceProxy.fetchAnnotations(DUMMY_PAGE_URL, (result) -> {
            Assert.assertNotNull(result);
            Assert.assertNotNull(result.getAnnotations());
            verifyAnnotations(result.getAnnotations());
        });
        verifyEndpointFetcherCalled(1, "my-endpoint.com?url=https%3A%2F%2Fwww.red.com%2Fpage1",
                new String[] {"Accept-Language", LocaleUtils.getDefaultLocaleListString()});
    }

    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:page_annotations_base_url/my-endpoint.com"})
    public void testFetchSinglePageAnnotationsUrlEscaping() {
        mServiceProxy.fetchAnnotations(
                new GURL("http://foo.bar?some=param with spaces"), (result) -> {});
        verifyEndpointFetcherCalled(1,
                "my-endpoint.com?url=http%3A%2F%2Ffoo.bar%2F%3Fsome%3Dparam%2520with%2520spaces",
                new String[] {"Accept-Language", LocaleUtils.getDefaultLocaleListString()});
    }

    private void verifyEndpointFetcherCalled(
            int numTimes, String expectedUrl, String[] expectedHeaders) {
        verify(mEndpointFetcherJniMock, times(numTimes))
                .nativeFetchChromeAPIKey(any(Profile.class), eq(expectedUrl), eq(EXPECTED_METHOD),
                        eq(EXPECTED_CONTENT_TYPE), anyString(), anyLong(), eq(expectedHeaders),
                        any(Callback.class));
    }

    private void mockEndpointResponse(String response) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[7];
                callback.onResult(new EndpointResponse(response));
                return null;
            }
        })
                .when(mEndpointFetcherJniMock)
                .nativeFetchChromeAPIKey(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), anyLong(), any(String[].class), any(Callback.class));
    }

    private void verifyAnnotations(List<PageAnnotation> annotations) {
        Assert.assertEquals(1, annotations.size());
        Assert.assertEquals(annotations.get(0).getType(), PageAnnotationType.BUYABLE_PRODUCT);
    }
}
