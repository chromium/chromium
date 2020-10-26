// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Unit tests for {@link HomepageManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HomepageManagerTest.ShadowHomepagePolicyManager.class,
                HomepageManagerTest.ShadowUrlUtilities.class})
public class HomepageManagerTest {
    /** Shadow for {@link HomepagePolicyManager}. */
    @Implements(HomepagePolicyManager.class)
    public static class ShadowHomepagePolicyManager {
        static String sHomepageUrl;

        @Implementation
        public static boolean isHomepageManagedByPolicy() {
            return true;
        }

        @Implementation
        public static String getHomepageUrl() {
            return sHomepageUrl;
        }
    }

    @Implements(UrlUtilities.class)
    static class ShadowUrlUtilities {
        @Implementation
        public static boolean isNTPUrl(String url) {
            return UrlConstants.NTP_URL.equals(url);
        }
    }

    @Test
    @SmallTest
    public void testIsHomepageNonNtp() {
        ShadowHomepagePolicyManager.sHomepageUrl = "";
        Assert.assertFalse(
                "Empty string should fall back to NTP", HomepageManager.isHomepageNonNtp());

        ShadowHomepagePolicyManager.sHomepageUrl = null;
        Assert.assertFalse("Null should fall back to the NTP", HomepageManager.isHomepageNonNtp());

        ShadowHomepagePolicyManager.sHomepageUrl = "www.example.com";
        Assert.assertTrue("Random web page is not the NTP", HomepageManager.isHomepageNonNtp());

        ShadowHomepagePolicyManager.sHomepageUrl = UrlConstants.NTP_URL;
        Assert.assertFalse("NTP should be considered the NTP", HomepageManager.isHomepageNonNtp());
    }
}
