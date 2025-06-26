// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileUtils;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.DialogMode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToBrowser;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToView;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.UrlErrorCode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.ViewToMediator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** The Mediator of the Custom Tile Edit Dialog. */
@NullMarked
class CustomTileEditMediator implements ViewToMediator {
    public static final String DEFAULT_URL_TEXT = "https://example.com";

    // Non-null => Edit shortcut dialog; null => Add shortcut dialog.
    private final @Nullable Tile mOriginalTile;

    private MediatorToView mViewDelegate;
    private MediatorToBrowser mBrowserDelegate;

    /**
     * Constructs a new CustomTileEditMediator.
     *
     * @param originalTile the tile being edited, or null if adding a new tile.
     */
    CustomTileEditMediator(@Nullable Tile originalTile) {
        mOriginalTile = originalTile;
    }

    /**
     * Assigns delegates for interacting with the Browser and the View.
     *
     * @param viewDelegate The interface to the View.
     * @param browserDelegate The interface to the Browser.
     */
    @Initializer
    void setDelegates(MediatorToView viewDelegate, MediatorToBrowser browserDelegate) {
        assert mViewDelegate == null;
        mViewDelegate = viewDelegate;
        assert mBrowserDelegate == null;
        mBrowserDelegate = browserDelegate;
    }

    // ViewToMediator implementation.
    @Override
    public void onUrlTextChanged(String urlText) {
        GURL url = convertUrlTextToGurl(urlText);
        @UrlErrorCode int urlErrorCode = validateUrl(url);
        boolean success = (urlErrorCode == UrlErrorCode.NONE);
        if (!success) {
            // Set URL error. This is automatically cleared on text edit.
            mViewDelegate.setUrlErrorByCode(urlErrorCode);
        }
        mViewDelegate.toggleSaveButton(success);
    }

    @Override
    public void onSave(String name, String urlText) {
        GURL url = convertUrlTextToGurl(urlText);
        @UrlErrorCode int urlErrorCode = validateUrl(url);
        boolean success = (urlErrorCode == UrlErrorCode.NONE);
        if (success) {
            String nameToUse = TileUtils.formatCustomTileName(name, url);
            if (!mBrowserDelegate.submitChange(nameToUse, url)) {
                // validateUrl() should have caught the error scenario, but handle again for
                // robustness.
                urlErrorCode = UrlErrorCode.DUPLICATE_URL;
                success = false;
            }
        }
        if (success) {
            mBrowserDelegate.closeEditDialog(true);
        } else {
            // Set URL error. This is automatically cleared on text edit.
            mViewDelegate.setUrlErrorByCode(urlErrorCode);
            // Set focus to the URL input field to facilitate URL update.
            mViewDelegate.focusOnUrl(false);
        }
    }

    @Override
    public void onCancel() {
        mBrowserDelegate.closeEditDialog(false);
    }

    /** Shows the edit dialog, populating it with the original tile's data if available. */
    void show() {
        mViewDelegate.setDialogMode(
                mOriginalTile == null ? DialogMode.ADD_SHORTCUT : DialogMode.EDIT_SHORTCUT);
        String name = "";
        String urlText = "";
        if (mOriginalTile != null) {
            // Edit shortcut: Populate with specified value.
            name = mOriginalTile.getTitle();
            urlText = mOriginalTile.getUrl().getPossiblyInvalidSpec();
        } else {
            // Add shortcut: Set default valid URL so no error is shown.
            urlText = DEFAULT_URL_TEXT;
        }
        mViewDelegate.setName(name);
        mViewDelegate.setUrlText(urlText);
        mBrowserDelegate.showEditDialog();

        if (mOriginalTile != null) {
            // Edit shortcut: Likely this is a name change, so focus on Name input field.
            mViewDelegate.addOnWindowFocusGainedTask(() -> mViewDelegate.focusOnName());
        } else {
            // Add shortcut: Likely pasting / inputting URL first, so focus on URL input field. To
            // make overwriting easier, select existing {@link #DEFAULT_URL_TEXT} text.
            mViewDelegate.addOnWindowFocusGainedTask(() -> mViewDelegate.focusOnUrl(true));
        }
    }

    /**
     * Converts user input string representing URL to a GURL. If no scheme is provided,
     * automatically prepend HTTPS scheme.
     */
    private GURL convertUrlTextToGurl(String urlText) {
        GURL url = new GURL(urlText);
        return url.getScheme().equals("") ? new GURL(UrlConstants.HTTPS_URL_PREFIX + urlText) : url;
    }

    private @UrlErrorCode int validateUrl(GURL url) {
        // If Edit shortcut then skip duplicate checks if URL didn't change. Note that GURL.equals()
        // conveniently ignores trailing "/".
        if (mOriginalTile == null || !mOriginalTile.getUrl().equals(url)) {
            if (!TileUtils.isValidCustomTileUrl(url)) {
                return UrlErrorCode.INVALID_URL;
            }
            if (mBrowserDelegate.isUrlDuplicate(url)) {
                return UrlErrorCode.DUPLICATE_URL;
            }
        }
        return UrlErrorCode.NONE;
    }
}
