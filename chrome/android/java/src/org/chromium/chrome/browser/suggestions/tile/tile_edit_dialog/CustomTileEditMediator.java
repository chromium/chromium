// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToBrowser;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToView;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.UrlErrorCode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.ViewToMediator;
import org.chromium.url.GURL;

/** The Mediator of the Most Visited Tile Edit Dialog. */
@NullMarked
class CustomTileEditMediator implements ViewToMediator {
    private final MediatorToBrowser mBrowserDelegate;
    private final MediatorToView mViewDelegate;
    private final @Nullable Tile mOriginalTile;

    /**
     * Constructs a new CustomTileEditMediator.
     *
     * @param browserDelegate The interface to the Browser.
     * @param viewDelegate The interface to the View.
     * @param originalTile the tile being edited, or null if adding a new tile.
     */
    CustomTileEditMediator(
            MediatorToBrowser browserDelegate,
            MediatorToView viewDelegate,
            @Nullable Tile originalTile) {
        mBrowserDelegate = browserDelegate;
        mViewDelegate = viewDelegate;
        mOriginalTile = originalTile;
    }

    // ViewToMediator implementation.
    @Override
    public void onUrlTextChanged(String urlText) {
        GURL url = new GURL(urlText);
        @UrlErrorCode int urlErrorCode = validateUrl(url);
        boolean success = (urlErrorCode == UrlErrorCode.NONE);
        if (!success) {
            // Set URL error. This is automatically cleared on text edit.
            mViewDelegate.setUrlErrorByCode(urlErrorCode);
        }
        mViewDelegate.toggleSaveButton(success);
    }

    @Override
    public void onSave(String title, String urlText) {
        GURL url = new GURL(urlText);
        @UrlErrorCode int urlErrorCode = validateUrl(url);
        boolean success = (urlErrorCode == UrlErrorCode.NONE);
        if (success && !mBrowserDelegate.submitChange(title, url)) {
            // validateUrl() should have caught the error scenario, but handle again for robustness.
            urlErrorCode = UrlErrorCode.DUPLICATE_URL;
            success = false;
        }
        if (success) {
            mBrowserDelegate.closeEditDialog(true);
        } else {
            // Set URL error. This is automatically cleared on text edit.
            mViewDelegate.setUrlErrorByCode(urlErrorCode);
            // Set focus to the URL input field to facilitate URL update.
            mViewDelegate.focusOnUrl();
        }
    }

    @Override
    public void onCancel() {
        mBrowserDelegate.closeEditDialog(false);
    }

    /** Shows the edit dialog, populating it with the original tile's data if available. */
    void show() {
        if (mOriginalTile != null) {
            mViewDelegate.setTitle(mOriginalTile.getTitle());
            mViewDelegate.setUrlText(mOriginalTile.getUrl().getPossiblyInvalidSpec());
        }
        mBrowserDelegate.showEditDialog();
    }

    private @UrlErrorCode int validateUrl(GURL url) {
        // If editing an existing tile, skip duplicate checks if URL didn't change. GURL.equals()
        // conveniently ignores trailing "/".
        if (mOriginalTile == null || !mOriginalTile.getUrl().equals(url)) {
            if (GURL.isEmptyOrInvalid(url)) {
                return UrlErrorCode.INVALID_URL;
            }
            if (mBrowserDelegate.isUrlDuplicate(url)) {
                return UrlErrorCode.DUPLICATE_URL;
            }
        }
        return UrlErrorCode.NONE;
    }
}
