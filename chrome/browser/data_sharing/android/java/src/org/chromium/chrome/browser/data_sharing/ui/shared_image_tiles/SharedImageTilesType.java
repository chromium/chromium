// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({SharedImageTilesType.DEFAULT, SharedImageTilesType.CLICKABLE})
@Retention(RetentionPolicy.SOURCE)
public @interface SharedImageTilesType {
    /** Standard SharedImageTiles behavior. */
    int DEFAULT = 0;

    /** Clickable with add person button. */
    int CLICKABLE = 1;
}
