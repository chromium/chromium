// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link OfflinePageOrigin}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OfflinePageOriginUnitTest {
    @Test
    public void testEncodeAsJson() {
        String appName = "abc.xyz";
        String[] signatures = new String[] {"deadbeef", "00c0ffee"};
        OfflinePageOrigin origin = new OfflinePageOrigin(appName, signatures);

        assertEquals("[\"abc.xyz\",[\"deadbeef\",\"00c0ffee\"]]", origin.encodeAsJsonString());
    }

    @Test
    public void testEquals() {
        String appName = "abc.xyz";
        String[] signature1 = new String[] {"deadbeef", "00c0ffee"};
        String[] signature2 = new String[] {"deadbeef", "fooba499"};
        OfflinePageOrigin origin1 = new OfflinePageOrigin(appName, signature1);
        OfflinePageOrigin origin2 = new OfflinePageOrigin(appName, signature2);
        OfflinePageOrigin origin3 = new OfflinePageOrigin("", signature1);
        OfflinePageOrigin origin4 = new OfflinePageOrigin(appName, signature1);

        assertFalse("Equivalent to null", origin1.equals(null));
        assertTrue("Not equivalent to self", origin1.equals(origin1));
        assertFalse("Equivalent when signatures not equal", origin1.equals(origin2));
        assertFalse("Equivalent when name not equal", origin1.equals(origin3));
        assertTrue("Equally created items not equal", origin1.equals(origin4));

        assertEquals("HashCode not equal to self", origin1.hashCode(), origin1.hashCode());
        assertEquals(
                "HashCode not equal when items are equal", origin1.hashCode(), origin4.hashCode());
    }
}
