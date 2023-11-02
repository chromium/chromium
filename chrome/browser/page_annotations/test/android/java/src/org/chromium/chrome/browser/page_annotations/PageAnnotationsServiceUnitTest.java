// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.page_annotations.PageAnnotation.PageAnnotationType;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.LinkedList;

/**
 * Tests for {@link PageAnnotationsServiceUnitTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageAnnotationsServiceUnitTest {
    private static final GURL DUMMY_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);

    private static final LinkedList<PageAnnotation> FAKE_ANNOTATIONS_LIST =
            new LinkedList<PageAnnotation>() {
                { add(new BuyableProductPageAnnotation(10000000L, "USD", "200")); }
            };

    @Mock
    private PageAnnotationsServiceProxy mServiceProxyMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private void mockServiceProxyResult(SinglePageAnnotationsServiceResponse response) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[1];
                callback.onResult(response);
                return null;
            }
        })
                .when(mServiceProxyMock)
                .fetchAnnotations(any(GURL.class), any(Callback.class));
    }

    @Test
    public void testReturnEmptyListOnNullResponse() {
        mockServiceProxyResult(null);
        PageAnnotationsService service = new PageAnnotationsService(mServiceProxyMock);
        service.getAnnotations(DUMMY_URL, (result) -> {
            Assert.assertNotNull(result);
            Assert.assertEquals(0, result.size());
        });
    }

    @Test
    public void testNullUrl() {
        mockServiceProxyResult(new SinglePageAnnotationsServiceResponse(FAKE_ANNOTATIONS_LIST));
        PageAnnotationsService service = new PageAnnotationsService(mServiceProxyMock);
        service.getAnnotations(null, (result) -> {
            Assert.assertNotNull(result);
            Assert.assertEquals(0, result.size());
        });
    }

    @Test
    public void testServerGeneratedAnnotations() {
        mockServiceProxyResult(new SinglePageAnnotationsServiceResponse(FAKE_ANNOTATIONS_LIST));
        PageAnnotationsService service = new PageAnnotationsService(mServiceProxyMock);
        service.getAnnotations(DUMMY_URL, (result) -> {
            Assert.assertNotNull(result);
            Assert.assertEquals(1, result.size());
            Assert.assertEquals(PageAnnotationType.BUYABLE_PRODUCT, result.get(0).getType());
        });
    }
}
