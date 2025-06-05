// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.url.GURL;

/** Utility class for the composeplate view. */
@NullMarked
public class ComposeplateUtils {
    private static final String VALID_URL_PROTOCOL = "https://";

    /** Returns the URL to open when clicking the composeplate view. */
    public static GURL getComposeplateURL() {
        String url = ChromeFeatureList.sAndroidComposeplateButtonUrl.getValue();
        if (url == null || !url.startsWith(VALID_URL_PROTOCOL)) {
            return new GURL(ChromeFeatureList.sAndroidComposeplateButtonUrl.getDefaultValue());
        }

        GURL gurl = new GURL(url);
        if (gurl.isValid()) {
            return gurl;
        }

        return new GURL(ChromeFeatureList.sAndroidComposeplateButtonUrl.getDefaultValue());
    }
}
