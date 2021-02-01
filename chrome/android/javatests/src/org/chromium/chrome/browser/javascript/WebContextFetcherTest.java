// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.javascript;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.javascript.WebContextFetcher.WebContextFetchResponse;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Unit tests for web context fetching java code.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class WebContextFetcherTest {
    /**
     * Test that converting map to json results in the correct output in various scenarios.
     */
    @Test
    @SmallTest
    public void testConvertJsonToResponse_Successful() {
        WebContextFetchResponse emptyResponse = WebContextFetcher.convertJsonToResponse("{}");
        Assert.assertTrue("Empty json dictionary does not return empty map.",
                emptyResponse.context.isEmpty());

        WebContextFetchResponse response =
                WebContextFetcher.convertJsonToResponse("{\"testing\": \"onetwothree\"}");
        Assert.assertEquals("Map with single key did not have correct key/val pair.", "onetwothree",
                response.context.get("testing"));
        Assert.assertEquals(
                "Map with single key had more than one key.", 1, response.context.size());
    }

    @Test
    @SmallTest
    public void testConvertJsonToResponse_AssertionErrorMalformed() {
        WebContextFetchResponse response =
                WebContextFetcher.convertJsonToResponse("14324asdfasc132434");

        Assert.assertEquals("Response with error key did not have correct value pair.",
                "Use JsonReader.setLenient(true) to accept malformed JSON at line 1 column 19",
                response.error);
        Assert.assertTrue("Response with error had filled context.", response.context.isEmpty());
    }

    @Test
    @SmallTest
    public void testConvertJsonToResponse_AssertionErrorNestedObject() {
        WebContextFetchResponse response = WebContextFetcher.convertJsonToResponse(
                "{\"nestedObject\": {\"nestedKey\": \"nestedVal\"}}");

        Assert.assertEquals("Response with error did not have correct message.",
                "Error reading JSON string value.", response.error);
        Assert.assertTrue("Response with error had filled context.", response.context.isEmpty());
    }

    @Test
    @SmallTest
    public void testConvertJsonToResponse_AssertionErrorNonStringVal() {
        WebContextFetchResponse response =
                WebContextFetcher.convertJsonToResponse("{\"integer\": 123}");

        Assert.assertEquals("Response with error  did not have correct message.",
                "Error reading JSON string value.", response.error);
        Assert.assertTrue("Response with error had filled context.", response.context.isEmpty());
    }

    @Test
    @SmallTest
    public void testConvertJsonToResponse_AssertionErrorArray() {
        WebContextFetchResponse response = WebContextFetcher.convertJsonToResponse(
                "{\"nestedArray\":[\"arrayVal1\", \"arrayVal2\"]}");

        Assert.assertEquals("Response with error key did not have correct message.",
                "Error reading JSON string value.", response.error);
        Assert.assertTrue("Response with error had filled context.", response.context.isEmpty());
    }
}
