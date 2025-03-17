// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToBrowser;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToView;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.UrlErrorCode;
import org.chromium.url.GURL;

/** Unit tests for {@link CustomTileEditMediator}. */
/** Tests for {@link MostVisitedTilesViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
public class CustomTileEditMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MediatorToBrowser mBrowserDelegate;
    @Mock private MediatorToView mViewDelegate;
    @Mock private Tile mOriginalTile;

    private CustomTileEditMediator mMediator;

    @Test
    public void testShowAddNewTile() {
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.show();

        verify(mViewDelegate, never()).setTitle(any());
        verify(mViewDelegate, never()).setUrlText(any());
        verify(mBrowserDelegate).showEditDialog();
    }

    @Test
    public void testShowEditExistingTile() {
        when(mOriginalTile.getTitle()).thenReturn("Test Title");
        when(mOriginalTile.getUrl()).thenReturn(new GURL("http://test.com"));
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, mOriginalTile);
        mMediator.show();

        verify(mViewDelegate).setTitle("Test Title");
        verify(mViewDelegate).setUrlText("http://test.com/");
        verify(mBrowserDelegate).showEditDialog();
    }

    @Test
    public void testOnUrlTextChangedValidUrl() {
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.onUrlTextChanged("http://valid.com");

        verify(mViewDelegate, never()).setUrlErrorByCode(anyInt());
        verify(mViewDelegate).toggleSaveButton(true);
    }

    @Test
    public void testOnUrlTextChangedInvalidUrl() {
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.onUrlTextChanged("invalid url");

        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.INVALID_URL);
        verify(mViewDelegate).toggleSaveButton(false);
    }

    @Test
    public void testOnUrlTextChangedDuplicateUrl() {
        when(mBrowserDelegate.isUrlDuplicate(any())).thenReturn(true);
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.onUrlTextChanged("http://duplicate.com");

        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.DUPLICATE_URL);
        verify(mViewDelegate).toggleSaveButton(false);
    }

    @Test
    public void testOnUrlTextChangedOriginalUrlUnchanged() {
        when(mOriginalTile.getUrl()).thenReturn(new GURL("http://original.com"));
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, mOriginalTile);
        mMediator.onUrlTextChanged("http://original.com");

        verify(mBrowserDelegate, never()).isUrlDuplicate(any());
        verify(mViewDelegate, never()).setUrlErrorByCode(anyInt());
        verify(mViewDelegate).toggleSaveButton(true);
    }

    @Test
    public void testOnSaveValidSubmit() {
        when(mBrowserDelegate.submitChange(any(), any())).thenReturn(true);
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.onSave("Test", "http://valid.com");

        verify(mBrowserDelegate).closeEditDialog(true);
        verify(mViewDelegate, never()).setUrlErrorByCode(anyInt());
    }

    @Test
    public void testOnSaveInvalidUrl() {
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.onSave("Test", "invalid url");

        verify(mBrowserDelegate, never()).closeEditDialog(anyBoolean());
        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.INVALID_URL);
        verify(mViewDelegate).focusOnUrl();
    }

    @Test
    public void testOnSaveDuplicateUrl() {
        when(mBrowserDelegate.submitChange(any(), any())).thenReturn(false);
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.onSave("Test", "http://duplicate.com");

        verify(mBrowserDelegate, never()).closeEditDialog(anyBoolean());
        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.DUPLICATE_URL);
        verify(mViewDelegate).focusOnUrl();
    }

    @Test
    public void testOnCancel() {
        mMediator = new CustomTileEditMediator(mBrowserDelegate, mViewDelegate, null);
        mMediator.onCancel();

        verify(mBrowserDelegate).closeEditDialog(false);
    }
}
