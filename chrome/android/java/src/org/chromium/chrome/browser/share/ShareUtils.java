// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** A collection of helper functions for sharing in a non static context. */
@NullMarked
public class ShareUtils {
    /**
     * Determines whether a tab is eligible to be shared.
     *
     * @param tab The tab being tested.
     * @return Whether the tab is eligible to be shared.
     */
    public static boolean shouldEnableShare(@Nullable Tab tab) {
        if (tab == null) {
            return false;
        }

        GURL url = tab.getUrl();

        boolean isChromeScheme =
                url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                        || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME);
        boolean isDataScheme = url.getScheme().equals(UrlConstants.DATA_SCHEME);
        boolean isDownloadedPdf = url.isValid() && PdfUtils.isDownloadedPdf(url.getSpec());

        return (!isChromeScheme && !isDataScheme) || isDownloadedPdf;
    }

    /** In the context of custom tabs, should the share be enabled. */
    public static boolean enableShareForAutomotive(boolean isCustomTabs) {
        return !isCustomTabs || !DeviceInfo.isAutomotive();
    }
}
