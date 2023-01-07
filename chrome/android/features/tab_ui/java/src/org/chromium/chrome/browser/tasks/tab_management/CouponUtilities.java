// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Utility class for coupons in Tab UI.
 */
public class CouponUtilities {
    /**
     * @return Whether commerce coupons feature flag is enabled.
     */
    public static boolean isCouponsOnTabsEnabled() {
        return ChromeFeatureList.sCommerceCoupons.isEnabled();
    }
}
