// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests AMP url handling in the CustomTab Toolbar.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabToolbarUnitTest {
    @Test
    public void testParsesPublisherFromAmp() {
        assertEquals("www.nyt.com",
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.AMP_URL)));
        assertEquals("www.nyt.com",
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.AMP_CACHE_URL)));
        assertEquals(JUnitTestGURLs.EXAMPLE_URL,
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
    }
}
