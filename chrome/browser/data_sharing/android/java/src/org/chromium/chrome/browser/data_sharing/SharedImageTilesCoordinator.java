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

    // The maximum amount of tiles that can show, including icon tile and count tile.
    private static final int MAX_TILES_TO_SHOW = 5;
    // The maximum number appearing on the count number tile.
    private static final int MAX_COUNT_TILE_NUMBER = 99;

    private final SharedImageTilesMediator mMediator;
    private final Context mContext;
    private final PropertyModel mModel;
    private final ViewGroup mView;

    /**
     * Constructor for SharedImageTilesCoordinator component.
     *
     * @param context The Android context used to inflate the views.
     */
    public SharedImageTilesCoordinator(Context context) {
        mModel = new PropertyModel.Builder(SharedImageTilesProperties.ALL_KEYS).build();
        mContext = context;

        mView =
                (ViewGroup)
                        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles, null);
        initializeSharedImageTiles();

        PropertyModelChangeProcessor.create(mModel, mView, SharedImageTilesViewBinder::bind);

        mMediator = new SharedImageTilesMediator(mModel);
    }

    /** Populate the shared_image_tiles container with the specific icons. */
    private void initializeSharedImageTiles() {
        // TODO(b/325533985): |tilesCount| should be replace by the actual number of icons
        // needed.
        int tilesCount = 8;
        int maxTilesToShowWithNumberTile = MAX_TILES_TO_SHOW - 1;
        boolean showNumberTile = tilesCount > MAX_TILES_TO_SHOW;
        int numIconTiles =
                showNumberTile
                        ? maxTilesToShowWithNumberTile
                        : Math.min(tilesCount, MAX_TILES_TO_SHOW);

        // Add icon tile(s).
        for (int i = 0; i < numIconTiles; i++) {
            appendIconTile();
        }

        // Add number tile.
        if (showNumberTile) {
            // Compute a count bubble.
            appendNumberTile(tilesCount - maxTilesToShowWithNumberTile);
        }
    }

    private void appendNumberTile(int remaining) {
        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles_count, mView, true);

        // Maximum tiles remaining count must be within 2 digits.
        mModel.set(
                SharedImageTilesProperties.REMAINING_TILES,
                Math.min(MAX_COUNT_TILE_NUMBER, remaining));
    }

    private void appendIconTile() {
        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles_icon, mView, true);
    }

    /** Get the view component of SharedImageTiles. */
    public @NonNull ViewGroup getView() {
        return mView;
    }

    /**
     * Update the shared image tiles' background color.
     *
     * @param backgroundColor The new background color to use.
     */
    public void updateBackgroundColor(int backgroundColor) {
        mModel.set(SharedImageTilesProperties.BACKGROUND_COLOR, backgroundColor);
    }
}
