// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;

/**
 * A helper class contains cached Lens feature flags and params.
 */
public class LensFeature {
    private static final String DISABLE_ON_INCOGNITO_PARAM_NAME = "disableOnIncognito";
    public static final BooleanCachedFieldTrialParameter
            DISABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_INCOGNITO = new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH, DISABLE_ON_INCOGNITO_PARAM_NAME,
                    true);

    private static final String ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_LOW_END_DEVICE_PARAM_NAME =
            "enableCameraAssistedSearchOnLowEndDevice";
    public static final BooleanCachedFieldTrialParameter
            ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_LOW_END_DEVICE =
                    new BooleanCachedFieldTrialParameter(
                            ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH,
                            ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_LOW_END_DEVICE_PARAM_NAME, false);

    private static final String MIN_AGSA_VERSION_LENS_CAMERA_ASSISTED_SEARCH_PARAM_NAME =
            "minAgsaVersionForLensCameraAssistedSearch";
    public static final StringCachedFieldTrialParameter
            MIN_AGSA_VERSION_LENS_CAMERA_ASSISTED_SEARCH = new StringCachedFieldTrialParameter(
                    ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH,
                    MIN_AGSA_VERSION_LENS_CAMERA_ASSISTED_SEARCH_PARAM_NAME, "12.13");

    private static final String SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH_PARAM_NAME =
            "searchBoxStartVariantForLensCameraAssistedSearch";
    public static final BooleanCachedFieldTrialParameter
            SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH =
                    new BooleanCachedFieldTrialParameter(
                            ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH,
                            SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH_PARAM_NAME, false);

    private static final String ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_TABLET_PARAM_NAME =
            "enableCameraAssistedSearchOnTablet";
    public static final BooleanCachedFieldTrialParameter
            ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_TABLET = new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH,
                    ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_TABLET_PARAM_NAME, false);
}
