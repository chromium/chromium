// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * A class to hold information about a background collection. This is the Java equivalent of the C++
 * CollectionInfo struct.
 */
@NullMarked
public class BackgroundCollection {
    // Collection identifier.
    public final String id;

    // Localized string of the collection name.
    public final String label;

    // URL to a preview image for the collection. Can point to untrusted content.
    public final GURL previewImageUrl;

    // Hash of collection id.
    public final int hash;

    public BackgroundCollection(String id, String label, GURL previewImageUrl, int hash) {
        this.id = id;
        this.label = label;
        this.previewImageUrl = previewImageUrl;
        this.hash = hash;
    }
}
