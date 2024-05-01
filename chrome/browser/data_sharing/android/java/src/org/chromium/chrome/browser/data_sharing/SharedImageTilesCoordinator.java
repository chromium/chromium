// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * A coordinator for SharedImageTiles component. This component is used to build a view and populate
 * shared image tilese details.
 */
public class SharedImageTilesCoordinator {

    // The maximum amount of tiles that can show, including icon tile and count tile.
    private static final int MAX_TILES_UI_LIMIT = 5;
    // The maximum number appearing on the count number tile.
    private static final int MAX_COUNT_TILE_NUMBER = 99;

    private final SharedImageTilesMediator mMediator;
    private final Context mContext;
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private int mAvailableTileCount;

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
        int maxTilesToShowWithNumberTile = MAX_TILES_UI_LIMIT - 1;
        boolean showNumberTile = mAvailableTileCount > MAX_TILES_UI_LIMIT;
        int numIconTiles =
                showNumberTile
                        ? MAX_TILES_UI_LIMIT - 1
                        : Math.min(mAvailableTileCount, MAX_TILES_UI_LIMIT);

        // Add icon tile(s).
        for (int i = 0; i < numIconTiles; i++) {
            appendIconTile();
        }

        // Add number tile.
        if (showNumberTile) {
            // Compute a count bubble.
            appendNumberTile(mAvailableTileCount - maxTilesToShowWithNumberTile);
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

    public void updateTilesCount(int count) {
        // TODO(b/325533985): |mAvailableTileCount| should be replace by the actual number of icons
        // needed.
        mAvailableTileCount = count;
        mView.removeAllViews();
        initializeSharedImageTiles();
    }

    public List<ViewGroup> getAllViews() {
        assert (mView.getChildCount() == mAvailableTileCount);
        List<ViewGroup> list = new ArrayList<>();
        for (int i = 0; i < mAvailableTileCount; i++) {
            View view = mView.getChildAt(i);
            list.add((ViewGroup) view);
        }
        return list;
    }

    public @NonNull Context getContext() {
        return mContext;
    }
}
