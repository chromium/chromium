// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.textfield.TextInputEditText;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.DialogMode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.UrlErrorCode;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.ViewToMediator;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link CustomTileEditView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTileEditViewUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ViewToMediator mMediatorDelegateMock;

    private Context mContext;
    private CustomTileEditView mView;
    private PropertyModel mDialogModel;
    private TextInputEditText mNameField;
    private TextInputEditText mUrlField;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        mView =
                (CustomTileEditView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.custom_tile_edit_layout, null);

        mNameField = mView.findViewById(R.id.name_field);
        assertNotNull(mNameField);
        mUrlField = mView.findViewById(R.id.url_field);
        assertNotNull(mUrlField);

        mView.setMediatorDelegate(mMediatorDelegateMock);
        mDialogModel = mView.getDialogModel();
        assertNotNull(mDialogModel);
    }

    @Test
    public void testGetDialogModel() {
        assertEquals(mDialogModel, mView.getDialogModel());
    }

    @Test
    public void testUrlTextChanged() {
        mUrlField.setText("test");
        verify(mMediatorDelegateMock).onUrlTextChanged("test");

        mUrlField.setText("http://example.com");
        verify(mMediatorDelegateMock).onUrlTextChanged("http://example.com");

        mUrlField.setText("");
        verify(mMediatorDelegateMock).onUrlTextChanged("");
    }

    @Test
    public void testPositiveButtonClick_CallsSaveDelegate() {
        String testName = "My Shortcut";
        String testUrlText = "https://example.com";
        mNameField.setText(testName);
        mUrlField.setText(testUrlText);
        verify(mMediatorDelegateMock).onUrlTextChanged(testUrlText);

        mView.onClick(mDialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        verify(mMediatorDelegateMock).onSave(testName, testUrlText);
        verifyNoMoreInteractions(mMediatorDelegateMock); // Ensure onCancel() wasn't called.
    }

    @Test
    public void testNegativeButtonClick_CallsCancelDelegate() {
        mView.onClick(mDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);

        verify(mMediatorDelegateMock).onCancel();
        verifyNoMoreInteractions(mMediatorDelegateMock); // Ensure onSave() wasn't called.
    }

    @Test
    public void testSetDialogMode_Add() {
        mView.setDialogMode(DialogMode.ADD_SHORTCUT);
        String expectedTitle = mContext.getString(R.string.edit_shortcut_title_add);
        assertEquals(expectedTitle, mDialogModel.get(ModalDialogProperties.TITLE));
    }

    @Test
    public void testSetDialogMode_Edit() {
        mView.setDialogMode(DialogMode.EDIT_SHORTCUT);
        String expectedTitle = mContext.getString(R.string.edit_shortcut_title_edit);
        assertEquals(expectedTitle, mDialogModel.get(ModalDialogProperties.TITLE));
    }

    @Test
    public void testSetName() {
        String testName = "Website Name";
        mView.setName(testName);
        assertEquals(testName, mNameField.getText().toString());
    }

    @Test
    public void testSetUrlText() {
        String testUrl = "https://www.example.com/path";
        mView.setUrlText(testUrl);
        assertEquals(testUrl, mUrlField.getText().toString());
    }

    @Test
    public void testSetUrlErrorByCode_None() {
        // Set an error first to ensure it gets cleared
        mUrlField.setError("Some previous error.");
        assertNotNull(mUrlField.getError());

        mView.setUrlErrorByCode(UrlErrorCode.NONE);
        assertNull(mUrlField.getError());
    }

    @Test
    public void testSetUrlErrorByCode_InvalidUrl() {
        mView.setUrlErrorByCode(UrlErrorCode.INVALID_URL);
        String expectedError = mContext.getString(R.string.ntp_custom_links_invalid_url);
        assertNotNull(mUrlField.getError());
        assertEquals(expectedError, mUrlField.getError().toString());
    }

    @Test
    public void testSetUrlErrorByCode_DuplicateUrl() {
        mView.setUrlErrorByCode(UrlErrorCode.DUPLICATE_URL);
        String expectedError = mContext.getString(R.string.ntp_custom_links_already_exists);
        assertNotNull(mUrlField.getError());
        assertEquals(expectedError, mUrlField.getError().toString());
    }

    @Test
    public void testToggleSaveButton_Enable() {
        mView.toggleSaveButton(true);
        assertFalse(mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    @Test
    public void testToggleSaveButton_Disable() {
        mView.toggleSaveButton(false);
        assertTrue(mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    @Test
    public void testFocusOnName() {
        // Focus on some other view first.
        mUrlField.requestFocus();
        assertFalse(mNameField.hasFocus());
        assertTrue(mUrlField.hasFocus());

        mView.focusOnName();
        assertTrue(mNameField.hasFocus());
        assertFalse(mUrlField.hasFocus());
    }

    @Test
    public void testFocusOnUrl() {
        String urlText = "https://example.com";
        mUrlField.setText(urlText);

        // Focus on some other view first.
        mNameField.requestFocus();
        assertTrue(mNameField.hasFocus());
        assertFalse(mUrlField.hasFocus());

        mView.focusOnUrl(false);
        assertTrue(mUrlField.hasFocus());
        assertFalse(mNameField.hasFocus());
        assertEquals(mUrlField.getSelectionStart(), mUrlField.getSelectionEnd());

        mView.focusOnUrl(true);
        assertTrue(mUrlField.hasFocus());
        assertFalse(mNameField.hasFocus());
        assertEquals(0, mUrlField.getSelectionStart());
        assertEquals(urlText.length(), mUrlField.getSelectionEnd());
    }
}
