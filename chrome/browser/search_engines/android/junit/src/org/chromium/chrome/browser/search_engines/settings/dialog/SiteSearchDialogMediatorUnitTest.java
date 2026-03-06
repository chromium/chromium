// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link SiteSearchDialogMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SiteSearchDialogMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private SiteSearchDialogSaveAction mSaveAction;
    @Mock private TemplateUrl mTemplateUrl;

    private PropertyModel mCustomViewModel;
    private PropertyModel mDialogModel;
    private SiteSearchDialogMediator mMediator;

    private static final String ERROR_MSG = "Not valid";

    @Before
    public void setUp() {
        when(mContext.getString(R.string.site_search_dialog_input_not_valid_error))
                .thenReturn(ERROR_MSG);

        mCustomViewModel =
                new PropertyModel(
                        SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE,
                        SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE,
                        SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE);
        mDialogModel = new PropertyModel(ModalDialogProperties.POSITIVE_BUTTON_DISABLED);

        mMediator =
                new SiteSearchDialogMediator(mContext, mModalDialogManager, mTemplateUrlService);
    }

    @Test
    public void testAddMode_Initialization() {
        mMediator.initialize(mCustomViewModel, mSaveAction, /* templateUrl= */ null);
        mMediator.show(mDialogModel);

        assertTrue(mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        verify(mModalDialogManager)
                .showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    @Test
    public void testEditMode_Initialization() {
        when(mTemplateUrl.getShortName()).thenReturn("Name");
        when(mTemplateUrl.getKeyword()).thenReturn("keyword");
        when(mTemplateUrl.getURL()).thenReturn("https://url.com");
        when(mTemplateUrl.getPrepopulatedId()).thenReturn(0);

        mMediator.initialize(mCustomViewModel, mSaveAction, mTemplateUrl);
        mMediator.show(mDialogModel);

        // In Edit mode, inputs start with default valid values. Thus, the save button should be
        // enabled.
        assertFalse(mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    @Test
    public void testAddMode_Validation_ValidInputs() {
        mMediator.initialize(mCustomViewModel, mSaveAction, /* templateUrl= */ null);
        mMediator.show(mDialogModel);

        when(mTemplateUrlService.isSearchEngineNameValid("name")).thenReturn(true);
        when(mTemplateUrlService.isSearchEngineKeywordValidToAdd("keyword")).thenReturn(true);
        when(mTemplateUrlService.isSearchEngineUrlValidToAdd("url")).thenReturn(true);

        mMediator.onNameChanged("name");
        mMediator.onKeywordChanged("keyword");
        mMediator.onUrlChanged("url");

        assertNull(mCustomViewModel.get(SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE));
        assertNull(mCustomViewModel.get(SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE));
        assertNull(mCustomViewModel.get(SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE));
        assertFalse(mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    @Test
    public void testAddMode_Validation_InvalidInputs() {
        mMediator.initialize(mCustomViewModel, mSaveAction, /* templateUrl= */ null);
        mMediator.show(mDialogModel);

        when(mTemplateUrlService.isSearchEngineNameValid("bad")).thenReturn(false);
        when(mTemplateUrlService.isSearchEngineKeywordValidToAdd("bad")).thenReturn(false);
        when(mTemplateUrlService.isSearchEngineUrlValidToAdd("bad")).thenReturn(false);

        mMediator.onNameChanged("bad");
        mMediator.onKeywordChanged("bad");
        mMediator.onUrlChanged("bad");

        assertEquals(
                ERROR_MSG,
                mCustomViewModel.get(SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE));
        assertEquals(
                ERROR_MSG,
                mCustomViewModel.get(SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE));
        assertEquals(
                ERROR_MSG,
                mCustomViewModel.get(SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE));
        assertTrue(mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    @Test
    public void testEditMode_Validation_UsesOriginalKeyword() {
        when(mTemplateUrl.getKeyword()).thenReturn("original");
        when(mTemplateUrl.getShortName()).thenReturn("Name");
        when(mTemplateUrl.getURL()).thenReturn("https://url.com");

        mMediator.initialize(mCustomViewModel, mSaveAction, mTemplateUrl);
        mMediator.show(mDialogModel);

        when(mTemplateUrlService.isSearchEngineKeywordValidToEdit("new_keyword", "original"))
                .thenReturn(true);
        when(mTemplateUrlService.isSearchEngineUrlValidToEdit("new_url", "original"))
                .thenReturn(true);

        mMediator.onKeywordChanged("new_keyword");
        mMediator.onUrlChanged("new_url");

        verify(mTemplateUrlService).isSearchEngineKeywordValidToEdit("new_keyword", "original");
        verify(mTemplateUrlService).isSearchEngineUrlValidToEdit("new_url", "original");
    }

    @Test
    public void testOnClick_PositiveButton() {
        mMediator.initialize(mCustomViewModel, mSaveAction, /* templateUrl= */ null);
        mMediator.show(mDialogModel);

        // Inputs are trimmed before saving
        mMediator.onNameChanged(" name ");
        mMediator.onKeywordChanged(" keyword ");
        mMediator.onUrlChanged(" url ");

        mMediator.onClick(mDialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        verify(mSaveAction).onSave("name", "keyword", "url");
        verify(mModalDialogManager)
                .dismissDialog(mDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    public void testOnClick_NegativeButton() {
        mMediator.initialize(mCustomViewModel, mSaveAction, /* templateUrl= */ null);
        mMediator.show(mDialogModel);

        mMediator.onClick(mDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);

        verify(mModalDialogManager)
                .dismissDialog(mDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    public void testDismiss() {
        mMediator.initialize(mCustomViewModel, mSaveAction, /* templateUrl= */ null);
        mMediator.show(mDialogModel);

        mMediator.dismiss();

        verify(mModalDialogManager)
                .dismissDialog(mDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }
}
