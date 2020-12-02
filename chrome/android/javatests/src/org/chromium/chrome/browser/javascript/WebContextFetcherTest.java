// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.javascript;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Map;

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
    public void testConvertJsonToMap_Successful() throws Exception {
        Map emptyMap = WebContextFetcher.convertJsonToMap("{}");
        Assert.assertTrue("Empty json dictionary does not return empty map.", emptyMap.isEmpty());

        Map mapWithSingleKey = WebContextFetcher.convertJsonToMap("{\"testing\": \"onetwothree\"}");
        Assert.assertEquals("Map with single key did not have correct key/val pair.", "onetwothree",
                mapWithSingleKey.get("testing"));
        Assert.assertEquals(
                "Map with single key had more than one key.", 1, mapWithSingleKey.size());
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testConvertJsonToMap_AssertionErrorMalformed() {
        Map unused = WebContextFetcher.convertJsonToMap("14324asdfasc132434");
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testConvertJsonToMap_AssertionErrorNestedObject() {
        Map unused = WebContextFetcher.convertJsonToMap(
                "{\"nestedObject\": {\"nestedKey\": \"nestedVal\"}}");
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testConvertJsonToMap_AssertionErrorNonStringVal() {
        Map unused = WebContextFetcher.convertJsonToMap("{\"integer\": 123}");
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testConvertJsonToMap_AssertionErrorArray() {
        Map unused = WebContextFetcher.convertJsonToMap(
                "{\"nestedArray\":[\"arrayVal1\", \"arrayVal2\"]}");
    }
}
