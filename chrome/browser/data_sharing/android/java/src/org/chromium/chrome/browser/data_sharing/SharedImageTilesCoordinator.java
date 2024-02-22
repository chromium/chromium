// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A coordinator for SharedImageTiles component. This component is used to build a view and populate
 * shared image tilese details.
 */
public class SharedImageTilesCoordinator {

    private static final int MAX_TILES_TO_SHOW = 5;

    private final SharedImageTilesMediator mMediator;
    private final Context mContext;
    private final ViewGroup mView;

    /**
     * Constructor for SharedImageTilesCoordinator component.
     *
     * @param context The Android context used to inflate the views.
     */
    public SharedImageTilesCoordinator(Context context) {
        PropertyModel model =
                new PropertyModel.Builder(SharedImageTilesProperties.ALL_KEYS).build();
        mContext = context;

        mView =
                (ViewGroup)
                        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles, null);
        initializeSharedImageTiles();

        PropertyModelChangeProcessor.create(model, mView, SharedImageTilesViewBinder::bind);

        mMediator = new SharedImageTilesMediator(model);
    }

    /** Populate the shared_image_tiles container with the specific icons. */
    private void initializeSharedImageTiles() {
        // Loop through all icons and add views.
        // TODO(b/325533985): |MAX_TILES_TO_SHOW| should be replace by the actual number of icons
        // needed.
        for (int i = 0; i < MAX_TILES_TO_SHOW; i++) {
            LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles_icon, mView, true);
        }
    }

    /** Get the view component of SharedImageTiles. */
    public @NonNull ViewGroup getView() {
        return mView;
    }
}
