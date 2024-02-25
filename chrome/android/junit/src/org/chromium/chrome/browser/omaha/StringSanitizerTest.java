// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

/** Tests the Omaha StringSanitizer. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class StringSanitizerTest {
    @Test
    @Feature({"Omaha"})
    public void testSanitizeStrings() {
        Assert.assertEquals("normal string", StringSanitizer.sanitize("Normal string"));
        Assert.assertEquals(
                "extra spaces string", StringSanitizer.sanitize("\nExtra,  spaces;  string "));
        Assert.assertEquals(
                "a quick brown fox jumped over the lazy dog",
                StringSanitizer.sanitize("  a\"quick;  brown,fox'jumped;over \nthe\rlazy\tdog\n"));
    }
}
