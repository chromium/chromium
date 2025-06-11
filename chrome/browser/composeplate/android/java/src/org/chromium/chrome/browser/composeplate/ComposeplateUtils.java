// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import org.chromium.base.LocaleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for the composeplate view. */
@NullMarked
public class ComposeplateUtils {
    private static final String LOCALE_US = "US";

    /**
     * Returns whether the composeplate can be enabled.
     *
     * @param isTablet Whether the device is a tablet.
     */
    public static boolean isComposeplateEnabled(boolean isTablet) {
        return ChromeFeatureList.sAndroidComposeplate.isEnabled()
                && !isTablet
                && (ChromeFeatureList.sAndroidComposeplateSkipLocaleCheck.getValue()
                        || LocaleUtils.getDefaultCountryCode().equals(LOCALE_US));
    }
}
