// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.widget.EditText;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class SiteSearchDialogCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;

    @Captor private ArgumentCaptor<PropertyModel> mDialogModelCaptor;

    private Context mContext;
    private SiteSearchDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new SiteSearchDialogCoordinator(mContext, mModalDialogManager, mTemplateUrlService);
    }

    @Test
    public void testShowAddDialog() {
        mCoordinator.showAddDialog();

        verify(mModalDialogManager)
                .showDialog(
                        mDialogModelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        PropertyModel dialogModel = mDialogModelCaptor.getValue();
        assertNotNull(dialogModel);

        // Title
        assertEquals(
                mContext.getString(R.string.site_search_dialog_title_add),
                dialogModel.get(ModalDialogProperties.TITLE));

        // Custom view
        assertNotNull(dialogModel.get(ModalDialogProperties.CUSTOM_VIEW));

        // Buttons
        assertEquals(
                mContext.getString(R.string.site_search_dialog_add),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mContext.getString(R.string.site_search_dialog_cancel),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        ModalDialogProperties.Controller controller =
                dialogModel.get(ModalDialogProperties.CONTROLLER);

        SiteSearchDialogMediator mediator =
                (SiteSearchDialogMediator) dialogModel.get(ModalDialogProperties.CONTROLLER);

        mediator.onNameChanged("name");
        mediator.onKeywordChanged("keyword");
        mediator.onUrlChanged("https://test.com");
        controller.onClick(dialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        verify(mTemplateUrlService).addSearchEngine("name", "keyword", "https://test.com");
    }

    @Test
    public void testShowEditDialog_NormalUrl() {
        when(mTemplateUrl.getKeyword()).thenReturn("keyword");
        when(mTemplateUrl.getShortName()).thenReturn("name");
        when(mTemplateUrl.getURL()).thenReturn("https://test.com");
        when(mTemplateUrl.getPrepopulatedId()).thenReturn(0);

        mCoordinator.showEditDialog(mTemplateUrl);

        verify(mModalDialogManager)
                .showDialog(
                        mDialogModelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        PropertyModel dialogModel = mDialogModelCaptor.getValue();
        assertNotNull(dialogModel);

        // Title
        assertEquals(
                mContext.getString(R.string.site_search_dialog_title_edit),
                dialogModel.get(ModalDialogProperties.TITLE));

        // Custom view
        assertNotNull(dialogModel.get(ModalDialogProperties.CUSTOM_VIEW));

        // Buttons
        assertEquals(
                mContext.getString(R.string.site_search_dialog_save),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mContext.getString(R.string.site_search_dialog_cancel),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        // Verify url can be edited
        EditText urlInput =
                dialogModel.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.url_input);
        assertTrue(urlInput.isEnabled());

        ModalDialogProperties.Controller controller =
                dialogModel.get(ModalDialogProperties.CONTROLLER);

        SiteSearchDialogMediator mediator =
                (SiteSearchDialogMediator) dialogModel.get(ModalDialogProperties.CONTROLLER);
        mediator.onNameChanged("new_name");
        mediator.onKeywordChanged("new_keyword");
        controller.onClick(dialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        verify(mTemplateUrlService)
                .editSearchEngine("keyword", "new_name", "new_keyword", "https://test.com");
    }

    @Test
    public void testShowEditDialog_PrepopulatedDisabledUrl() {
        when(mTemplateUrl.getKeyword()).thenReturn("keyword");
        when(mTemplateUrl.getShortName()).thenReturn("name");
        when(mTemplateUrl.getURL()).thenReturn("https://test.com");
        when(mTemplateUrl.getPrepopulatedId()).thenReturn(1);

        mCoordinator.showEditDialog(mTemplateUrl);

        verify(mModalDialogManager)
                .showDialog(
                        mDialogModelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        PropertyModel dialogModel = mDialogModelCaptor.getValue();

        EditText urlInput =
                dialogModel.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.url_input);
        assertFalse(urlInput.isEnabled());
    }

    @Test
    public void testDismiss() {
        mCoordinator.showAddDialog();

        verify(mModalDialogManager)
                .showDialog(
                        mDialogModelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        mCoordinator.dismiss();

        verify(mModalDialogManager)
                .dismissDialog(
                        mDialogModelCaptor.getValue(),
                        DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }
}
