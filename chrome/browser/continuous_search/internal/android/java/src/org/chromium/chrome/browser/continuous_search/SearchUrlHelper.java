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
     * Checks whether the provided url is valid, the host is "www.google.<TLD>" with a valid TLD and
     * has an HTTP or HTTPS scheme. Returns false if the url doesn't use the standard port for its
     * scheme (80 for HTTP, 443 for HTTPS).
     * @param url the url to check the criteria against.
     * @return true if url satisfies all the requirements above.
     */
    public static boolean isGoogleDomainUrl(GURL url) {
        return SearchUrlHelperJni.get().isGoogleDomainUrl(url);
    }

    /**
     * Gets the query of the provided url if it is a SRP URL and shows the "All" or "News" tab
     * results.
     * @param url The url to try to extract the query from.
     * @return the query of the url if the url is for a SRP or null otherwise.
     */
    public static String getQueryIfValidSrpUrl(GURL url) {
        return SearchUrlHelperJni.get().getQueryIfValidSrpUrl(url);
    }

    /**
     * Gets the result category from the given URL
     * @param url the url to get the category from
     * @return the appropriate category
     */
    public static @PageCategory int getSrpPageCategoryFromUrl(GURL url) {
        return SearchUrlHelperJni.get().getSrpPageCategoryFromUrl(url);
    }

    /**
     * Returns the appropriate histogram suffix (".Organic", ".News") based on the given page
     * category.
     * @param category the page category to determine the histogram suffix with
     * @return the suffix string
     */
    public static String getHistogramSuffixForPageCategory(@PageCategory int category) {
        switch (category) {
            case PageCategory.ORGANIC_SRP:
                return ".Organic";
            case PageCategory.NEWS_SRP:
                return ".News";
            default:
                assert false : "No histogram suffix for type " + category;
                return null;
        }
    }

    @NativeMethods
    interface Natives {
        boolean isGoogleDomainUrl(GURL url);
        String getQueryIfValidSrpUrl(GURL url);
        int getSrpPageCategoryFromUrl(GURL url);
    }
}
