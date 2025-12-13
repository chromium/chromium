// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

import java.util.List;

/**
 * A class to hold information about an image in a background collection. This is the Java
 * equivalent of the C++ CollectionImage struct.
 */
@NullMarked
public class CollectionImage {
    // Collection id of the image;
    public final String collectionId;

    // URL of image. Can point to untrusted content.
    public final GURL imageUrl;

    // URL to a preview of the image. Can point to untrusted content.
    public final GURL previewImageUrl;

    // Human readable attributions of the background image.
    public final List<String> attribution;

    // URL associated with the background image. Used for href.
    public final GURL attributionUrl;

    public CollectionImage(
            String collectionId,
            GURL imageUrl,
            GURL previewImageUrl,
            List<String> attribution,
            GURL attributionUrl) {
        this.collectionId = collectionId;
        this.imageUrl = imageUrl;
        this.previewImageUrl = previewImageUrl;
        this.attribution = attribution;
        this.attributionUrl = attributionUrl;
    }
}
