// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

/**
 * Helper class for handling SRP URLs.
 */
@JNINamespace("continuous_search")
public class SearchUrlHelper {
    private SearchUrlHelper() {}

    /**
     * Gets the query of the provided url if it is a SRP URL.
     * @param url The url to try to extract the query from.
     * @return the query of the url if the url is for a SRP or null otherwise.
     */
    public static String getQueryIfSrpUrl(GURL url) {
        return SearchUrlHelperJni.get().getQueryIfSrpUrl(url);
    }

    @NativeMethods
    interface Natives {
        String getQueryIfSrpUrl(GURL url);
    }
}
