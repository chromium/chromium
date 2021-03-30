// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/**
 * This class calls into native to request that a give tab starts site isolation.
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
