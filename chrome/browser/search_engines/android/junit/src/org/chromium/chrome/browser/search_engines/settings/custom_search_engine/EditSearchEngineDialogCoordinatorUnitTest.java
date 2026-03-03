// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.EditText;

import com.google.android.material.textfield.TextInputLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link EditSearchEngineDialogCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EditSearchEngineDialogCoordinatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private TemplateUrl mTemplateUrl;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private ModalDialogManager mModalDialogManager;

    private Context mContext;
    private EditSearchEngineDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new EditSearchEngineDialogCoordinator(
                        mContext, mModalDialogManager, mTemplateUrlService);

        when(mTemplateUrl.getShortName()).thenReturn("name");
        when(mTemplateUrl.getKeyword()).thenReturn("keyword");
        when(mTemplateUrl.getURL()).thenReturn("https://example.com/search?q=%s");

        when(mTemplateUrlService.isSearchEngineNameValid(any())).thenReturn(true);
        when(mTemplateUrlService.isSearchEngineKeywordValidToEdit(any(), any())).thenReturn(true);
        when(mTemplateUrlService.isSearchEngineUrlValidToEdit(any(), any())).thenReturn(true);
    }

    @Test
    public void testDialogIsShown() {
        mCoordinator.show(mTemplateUrl);

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        PropertyModel model = modelCaptor.getValue();

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        EditText nameInput = customView.findViewById(R.id.name_input);
        EditText shortcutInput = customView.findViewById(R.id.shortcut_input);
        EditText urlInput = customView.findViewById(R.id.url_input);

        assertEquals("name", nameInput.getText().toString());
        assertEquals("keyword", shortcutInput.getText().toString());
        assertEquals("https://example.com/search?q=%s", urlInput.getText().toString());
    }

    @Test
    public void testDialogIsShown_Prepopulated() {
        when(mTemplateUrl.getPrepopulatedId()).thenReturn(1);
        mCoordinator.show(mTemplateUrl);

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));

        PropertyModel model = modelCaptor.getValue();
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        EditText urlInput = customView.findViewById(R.id.url_input);

        assertFalse(urlInput.isEnabled());
    }

    @Test
    public void testDialogPositiveButton() {
        mCoordinator.show(mTemplateUrl);

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), any(Integer.class));
        PropertyModel model = modelCaptor.getValue();

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        EditText nameInput = customView.findViewById(R.id.name_input);
        nameInput.setText("new name");

        assertFalse(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mTemplateUrlService)
                .editSearchEngine(
                        "keyword", "new name", "keyword", "https://example.com/search?q=%s");
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    public void testSaveButtonState_DisabledWhenEmpty() {
        mCoordinator.show(mTemplateUrl);

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        EditText nameInput = customView.findViewById(R.id.name_input);
        TextInputLayout nameLayout = customView.findViewById(R.id.name_input_layout);

        nameInput.setText("");

        assertTrue(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        assertNull(nameLayout.getError());
    }

    @Test
    public void testSaveButtonState_DisabledAndShowsErrorWhenInvalid() {
        mCoordinator.show(mTemplateUrl);

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        EditText nameInput = customView.findViewById(R.id.name_input);
        TextInputLayout nameLayout = customView.findViewById(R.id.name_input_layout);
        EditText shortcutInput = customView.findViewById(R.id.shortcut_input);
        TextInputLayout shortcutLayout = customView.findViewById(R.id.shortcut_input_layout);

        when(mTemplateUrlService.isSearchEngineNameValid(any())).thenReturn(false);
        when(mTemplateUrlService.isSearchEngineKeywordValidToEdit(any(), any())).thenReturn(false);

        nameInput.setText("   ");
        shortcutInput.setText("   ");
        assertEquals(
                mContext.getString(R.string.site_search_dialog_input_not_valid_error),
                nameLayout.getError());
        assertEquals(
                mContext.getString(R.string.site_search_dialog_input_not_valid_error),
                shortcutLayout.getError());
    }

    @Test
    public void testDialogNegativeButton() {
        mCoordinator.show(mTemplateUrl);

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), any(Integer.class));
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        controller.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }
}
