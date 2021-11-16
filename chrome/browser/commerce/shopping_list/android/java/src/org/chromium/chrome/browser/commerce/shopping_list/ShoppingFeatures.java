// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.shopping_list;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Self-documenting feature class for shopping.  */
public class ShoppingFeatures {
    /** Returns whether shopping is enabled. */
    public static boolean isShoppingListEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.SHOPPING_LIST);
    }
}