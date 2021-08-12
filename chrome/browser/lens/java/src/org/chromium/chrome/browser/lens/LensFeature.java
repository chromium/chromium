// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * A helper class contains cached Lens feature flags and params.
 */
public class LensFeature {
    private static final String SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH_PARAM_NAME =
            "searchBoxStartVariantForLensCameraAssistedSearch";
    public static final BooleanCachedFieldTrialParameter
            SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH =
                    new BooleanCachedFieldTrialParameter(
                            ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH,
                            SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH_PARAM_NAME, false);
}
