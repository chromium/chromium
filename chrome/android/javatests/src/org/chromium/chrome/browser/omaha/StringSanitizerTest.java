// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests the Omaha StringSanitizer. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class StringSanitizerTest {
    @Test
    @SmallTest
    @Feature({"Omaha"})
    public void testSanitizeStrings() {
        Assert.assertEquals("normal string", StringSanitizer.sanitize("Normal string"));
        Assert.assertEquals(
                "extra spaces string", StringSanitizer.sanitize("\nExtra,  spaces;  string "));
        Assert.assertEquals("a quick brown fox jumped over the lazy dog",
                StringSanitizer.sanitize("  a\"quick;  brown,fox'jumped;over \nthe\rlazy\tdog\n"));
    }
}
