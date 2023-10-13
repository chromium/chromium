// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/**
 * This class calls into native to request that a given tab starts site
 * isolation for the provided url's site.  Note that the site will be isolated
 * with a USER_TRIGGERED IsolatedOriginSource.
 */
public class SiteIsolator {
    private SiteIsolator() {}

    public static void startIsolatingSite(GURL url) {
        Profile profile = Profile.getLastUsedRegularProfile();
        SiteIsolatorJni.get().startIsolatingSite(profile, url);
    }

    @NativeMethods
    interface Natives {
        void startIsolatingSite(Profile profile, GURL url);
    }
}
