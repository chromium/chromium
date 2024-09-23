// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.PropertyModel;

/** Contains the logic to set the state of the model and react to clicks. */
class SharedImageTilesMediator {

    private final @NonNull PropertyModel mModel;

    /**
     * @param model the {@link PropertyModel} for this UI.
     */
    public SharedImageTilesMediator(@NonNull PropertyModel model) {
        mModel = model;
    }

    // TODO(b/325533985): Handle clicks and fill out this class.
}
