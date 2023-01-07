// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controls how Video previews will automatically play. This is also used for histograms and should
 * therefore be treated as append-only. See FeedVideoPreviewsPreferenceUserActions in
 * tools/metrics/histograms/enums.xml.
 */
@IntDef({VideoPreviewsType.NEVER, VideoPreviewsType.WIFI, VideoPreviewsType.WIFI_AND_MOBILE_DATA})
@Retention(RetentionPolicy.SOURCE)
public @interface VideoPreviewsType {
    // Values are used for indexing tables - should start from 0 and can't have gaps.
    int NEVER = 0;
    int WIFI = 1;
    int WIFI_AND_MOBILE_DATA = 2;

    int NUM_ENTRIES = 3;
}
