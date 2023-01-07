// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;

/** Flag configuration for Page Annotations Service. */
public class PageAnnotationsServiceConfig {
    private static final String BASE_URL_PARAM = "page_annotations_base_url";
    private static final String DEFAULT_BASE_URL = "https://memex-pa.googleapis.com/v1/annotations";

    public static final StringCachedFieldTrialParameter PAGE_ANNOTATIONS_BASE_URL =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.PAGE_ANNOTATIONS_SERVICE, BASE_URL_PARAM, DEFAULT_BASE_URL);
}