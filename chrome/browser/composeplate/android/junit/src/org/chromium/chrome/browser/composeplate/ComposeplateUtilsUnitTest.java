// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.url.GURL;

/** Unit tests for {@link ComposeplateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ComposeplateUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testGetComposeplateURL() {
        String defaultUrl = "about:blank";
        GURL defaultGurl = new GURL(defaultUrl);
        assertEquals(defaultUrl, ChromeFeatureList.sAndroidComposeplateButtonUrl.getDefaultValue());

        ChromeFeatureList.sAndroidComposeplateButtonUrl.setForTesting("foo.com");
        assertTrue(defaultGurl.equals(ComposeplateUtils.getComposeplateURL()));

        String validUrl = "http://foo.com";
        GURL validGurl = new GURL(validUrl);
        ChromeFeatureList.sAndroidComposeplateButtonUrl.setForTesting(validUrl);
        assertTrue(defaultGurl.equals(ComposeplateUtils.getComposeplateURL()));

        validUrl = "https://foo.com";
        validGurl = new GURL(validUrl);
        ChromeFeatureList.sAndroidComposeplateButtonUrl.setForTesting(validUrl);
        assertTrue(validGurl.equals(ComposeplateUtils.getComposeplateURL()));
    }
}
