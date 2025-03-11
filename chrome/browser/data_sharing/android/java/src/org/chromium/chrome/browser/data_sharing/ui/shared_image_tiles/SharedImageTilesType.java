// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({SharedImageTilesType.DEFAULT, SharedImageTilesType.SMALL})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface SharedImageTilesType {
    /** Standard SharedImageTiles behavior. */
    int DEFAULT = 0;

    /** Smaller variant. */
    int SMALL = 1;
}
