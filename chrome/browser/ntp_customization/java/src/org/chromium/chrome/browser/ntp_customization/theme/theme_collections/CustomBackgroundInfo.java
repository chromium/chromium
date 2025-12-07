// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/** A class to hold information about a custom background. */
@NullMarked
public class CustomBackgroundInfo {
    public final GURL backgroundUrl;
    public final String collectionId;
    public final boolean isUploadedImage;
    public final boolean isDailyRefreshEnabled;

    public CustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled) {
        this.backgroundUrl = backgroundUrl;
        this.collectionId = collectionId;
        this.isUploadedImage = isUploadedImage;
        this.isDailyRefreshEnabled = isDailyRefreshEnabled;
    }
}
