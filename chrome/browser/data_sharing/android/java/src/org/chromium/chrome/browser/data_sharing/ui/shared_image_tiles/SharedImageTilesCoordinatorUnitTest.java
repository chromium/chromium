// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.widget.ChromeImageButton;

/** Unit test for {@link SharedImageTilesCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedImageTilesCoordinatorUnitTest {
    private Context mContext;
    private SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private SharedImageTilesView mView;
    private ChromeImageButton mButtonTileView;
    private TextView mCountTileView;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mSharedImageTilesCoordinator =
                new SharedImageTilesCoordinator(
                        mContext, SharedImageTilesType.DEFAULT, SharedImageTilesColor.DEFAULT);
        mView = mSharedImageTilesCoordinator.getView();
        mButtonTileView = mView.findViewById(R.id.shared_image_tiles_add);
        mCountTileView = mView.findViewById(R.id.tiles_count);
    }

    private void initialize(@SharedImageTilesType int type, @SharedImageTilesColor int color) {
        mSharedImageTilesCoordinator = new SharedImageTilesCoordinator(mContext, type, color);
        mView = mSharedImageTilesCoordinator.getView();
        mButtonTileView = mView.findViewById(R.id.shared_image_tiles_add);
        mCountTileView = mView.findViewById(R.id.tiles_count);
    }

    private void verifyViews(int buttonVisibility, int countVisibility, int iconViewCount) {
        assertEquals(mButtonTileView.getVisibility(), buttonVisibility);
        assertEquals(mCountTileView.getVisibility(), countVisibility);
        assertEquals(mSharedImageTilesCoordinator.getAllIconViews().size(), iconViewCount);
    }

    @Test
    public void testInitialize() {
        assertNotNull(mSharedImageTilesCoordinator.getView());
    }

    @Test
    public void testDefaultTheme() {
        initialize(SharedImageTilesType.DEFAULT, SharedImageTilesColor.DEFAULT);
        // Default theme should have the following view logic:
        // 0 tile count: None
        // 1 tile count: Tile
        // 2 tile count: Tile Tile
        // 3 tile count: Tile Tile Tile
        // 4 tile count: Tile Tile +2
        // etc
        verifyViews(View.GONE, View.GONE, /* iconViewCount= */ 0);

        mSharedImageTilesCoordinator.updateTilesCount(1);
        verifyViews(View.GONE, View.GONE, /* iconViewCount= */ 1);

        mSharedImageTilesCoordinator.updateTilesCount(2);
        verifyViews(View.GONE, View.GONE, /* iconViewCount= */ 2);

        mSharedImageTilesCoordinator.updateTilesCount(3);
        verifyViews(View.GONE, View.GONE, /* iconViewCount= */ 3);

        mSharedImageTilesCoordinator.updateTilesCount(4);
        verifyViews(View.GONE, View.VISIBLE, /* iconViewCount= */ 2);
    }

    @Test
    public void testClickableTheme() {
        initialize(SharedImageTilesType.CLICKABLE, SharedImageTilesColor.DEFAULT);
        // Clickable theme should have the following view logic:
        // 1 tile: Tile (add user)
        // 2 tiles Tile Tile (add user)
        // 3 tiles: Tile Tile Tile
        // 4 tiles: Tile Tile +2
        // etc
        verifyViews(View.GONE, View.GONE, /* iconViewCount= */ 0);

        mSharedImageTilesCoordinator.updateTilesCount(1);
        verifyViews(View.VISIBLE, View.GONE, /* iconViewCount= */ 1);

        mSharedImageTilesCoordinator.updateTilesCount(2);
        verifyViews(View.VISIBLE, View.GONE, /* iconViewCount= */ 2);

        mSharedImageTilesCoordinator.updateTilesCount(3);
        verifyViews(View.GONE, View.GONE, /* iconViewCount= */ 3);

        mSharedImageTilesCoordinator.updateTilesCount(4);
        verifyViews(View.GONE, View.VISIBLE, /* iconViewCount= */ 2);
    }
}
