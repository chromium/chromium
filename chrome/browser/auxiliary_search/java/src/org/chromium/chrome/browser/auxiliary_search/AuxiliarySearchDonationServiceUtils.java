// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility methods for the Auxiliary Search Donation Service (browsing data donation). */
@NullMarked
public class AuxiliarySearchDonationServiceUtils {
    private AuxiliarySearchDonationServiceUtils() {}

    /** Returns whether browsing data donation is supported on the device. */
    public static boolean isBrowsingDataDonationSupported() {
        AuxiliarySearchHooks hooks = ServiceLoaderUtil.maybeCreate(AuxiliarySearchHooks.class);
        return hooks != null && hooks.isBrowsingDataDonationSupported();
    }

    /** Returns whether browsing data donation is enabled. */
    public static boolean isBrowsingDataDonationEnabled() {
        return ChromeFeatureList.sAuxiliarySearchHistoryDonation.isEnabled()
                && isBrowsingDataDonationSupported();
    }
}
