// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.base.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoUtils.LogoSizeForLogoPolish;

/**
 * Flag configuration for Start Surface. Source of truth for whether it should be enabled and which
 * variation should be used.
 */
public class StartSurfaceConfiguration {
    private static final String LOGO_POLISH_LARGE_SIZE_PARAM = "polish_logo_size_large";
    public static final BooleanCachedFieldTrialParameter LOGO_POLISH_LARGE_SIZE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LOGO_POLISH, LOGO_POLISH_LARGE_SIZE_PARAM, false);

    private static final String LOGO_POLISH_MEDIUM_SIZE_PARAM = "polish_logo_size_medium";
    public static final BooleanCachedFieldTrialParameter LOGO_POLISH_MEDIUM_SIZE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LOGO_POLISH, LOGO_POLISH_MEDIUM_SIZE_PARAM, false);

    /** Returns whether logo polish flag is enabled in the given context. */
    public static boolean isLogoPolishEnabled() {
        return ChromeFeatureList.sLogoPolish.isEnabled();
    }

    /**
     * Returns whether logo is Google doodle and logo polish is enabled in the given context.
     *
     * @param isLogoDoodle True if the current logo is Google doodle.
     */
    public static boolean isLogoPolishEnabledWithGoogleDoodle(boolean isLogoDoodle) {
        return isLogoDoodle && isLogoPolishEnabled();
    }

    /**
     * Returns the logo size to use when logo polish is enabled. When logo polish is disabled, the
     * return value should be invalid.
     */
    public static @LogoSizeForLogoPolish int getLogoSizeForLogoPolish() {
        if (StartSurfaceConfiguration.LOGO_POLISH_LARGE_SIZE.getValue()) {
            return LogoSizeForLogoPolish.LARGE;
        }

        if (StartSurfaceConfiguration.LOGO_POLISH_MEDIUM_SIZE.getValue()) {
            return LogoSizeForLogoPolish.MEDIUM;
        }

        return LogoSizeForLogoPolish.SMALL;
    }
}
