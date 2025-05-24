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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.DialogMode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToBrowser;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToView;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.UrlErrorCode;
import org.chromium.url.GURL;

/** Unit tests for {@link CustomTileEditMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
public class CustomTileEditMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MediatorToBrowser mBrowserDelegate;
    @Mock private MediatorToView mViewDelegate;
    @Mock private Tile mOriginalTile;

    @Test
    public void testShowAddNewTile() {
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.show();

        verify(mViewDelegate).setDialogMode(DialogMode.ADD_SHORTCUT);
        verify(mViewDelegate).setName("");
        verify(mViewDelegate).setUrlText(CustomTileEditMediator.DEFAULT_URL_TEXT);
        verify(mViewDelegate).focusOnUrl(true);
        verify(mBrowserDelegate).showEditDialog();
    }

    @Test
    public void testShowEditExistingTile() {
        when(mOriginalTile.getTitle()).thenReturn("Test Name");
        when(mOriginalTile.getUrl()).thenReturn(new GURL("http://test.com"));
        CustomTileEditMediator mediator = createAndSetupMediator(mOriginalTile);
        mediator.show();

        verify(mViewDelegate).setDialogMode(DialogMode.EDIT_SHORTCUT);
        verify(mViewDelegate).setName("Test Name");
        verify(mViewDelegate).setUrlText("http://test.com/");
        verify(mViewDelegate).focusOnName();
        verify(mBrowserDelegate).showEditDialog();
    }

    @Test
    public void testOnUrlTextChangedValidUrl() {
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.onUrlTextChanged("http://valid.com");

        verify(mViewDelegate, never()).setUrlErrorByCode(anyInt());
        verify(mViewDelegate).toggleSaveButton(true);
    }

    @Test
    public void testOnUrlTextChangedInvalidUrl() {
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.onUrlTextChanged("invalid url");

        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.INVALID_URL);
        verify(mViewDelegate).toggleSaveButton(false);
    }

    @Test
    public void testOnUrlTextChangedDuplicateUrl() {
        when(mBrowserDelegate.isUrlDuplicate(any())).thenReturn(true);
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.onUrlTextChanged("http://duplicate.com");

        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.DUPLICATE_URL);
        verify(mViewDelegate).toggleSaveButton(false);
    }

    @Test
    public void testOnUrlTextChangedOriginalUrlUnchanged() {
        when(mOriginalTile.getUrl()).thenReturn(new GURL("http://original.com"));
        CustomTileEditMediator mediator = createAndSetupMediator(mOriginalTile);
        mediator.onUrlTextChanged("http://original.com");

        verify(mBrowserDelegate, never()).isUrlDuplicate(any());
        verify(mViewDelegate, never()).setUrlErrorByCode(anyInt());
        verify(mViewDelegate).toggleSaveButton(true);
    }

    @Test
    public void testOnSaveValidSubmit() {
        when(mBrowserDelegate.submitChange(any(), any())).thenReturn(true);
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.onSave("Test", "http://valid.com");

        verify(mBrowserDelegate).closeEditDialog(true);
        verify(mViewDelegate, never()).setUrlErrorByCode(anyInt());
    }

    @Test
    public void testOnSaveInvalidUrl() {
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.onSave("Test", "invalid url");

        verify(mBrowserDelegate, never()).closeEditDialog(anyBoolean());
        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.INVALID_URL);
        verify(mViewDelegate).focusOnUrl(false);
    }

    @Test
    public void testOnSaveDuplicateUrl() {
        when(mBrowserDelegate.submitChange(any(), any())).thenReturn(false);
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.onSave("Test", "http://duplicate.com");

        verify(mBrowserDelegate, never()).closeEditDialog(anyBoolean());
        verify(mViewDelegate).setUrlErrorByCode(UrlErrorCode.DUPLICATE_URL);
        verify(mViewDelegate).focusOnUrl(false);
    }

    @Test
    public void testOnCancel() {
        CustomTileEditMediator mediator = createAndSetupMediator(/* originalTile= */ null);
        mediator.onCancel();

        verify(mBrowserDelegate).closeEditDialog(false);
    }

    /**
     * Helper to create the Mediator, assuming mocks have been set up.
     *
     * @param originalTile The tile to edit, or null to add a new tile.
     */
    private CustomTileEditMediator createAndSetupMediator(@Nullable Tile originalTile) {
        CustomTileEditMediator mediator = new CustomTileEditMediator(originalTile);
        mediator.setDelegates(mViewDelegate, mBrowserDelegate);
        return mediator;
    }
}
