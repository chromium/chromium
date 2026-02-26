// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.EditText;

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
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AddSearchEngineDialogCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AddSearchEngineDialogCoordinatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private ModalDialogManager mModalDialogManager;

    private Context mContext;
    private AddSearchEngineDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new AddSearchEngineDialogCoordinator(
                        mContext, mModalDialogManager, mTemplateUrlService);
    }

    @Test
    public void testDialogIsShown() {
        mCoordinator.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
    }

    @Test
    public void testDialogPositiveButton() {
        mCoordinator.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), any(Integer.class));
        PropertyModel model = modelCaptor.getValue();

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        EditText nameInput = customView.findViewById(R.id.name_input);
        nameInput.setText("name");
        EditText shortcutInput = customView.findViewById(R.id.shortcut_input);
        shortcutInput.setText("shortcut");
        EditText urlInput = customView.findViewById(R.id.url_input);
        urlInput.setText("https://example.com/search?q=%s");

        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mTemplateUrlService)
                .addSearchEngine("name", "shortcut", "https://example.com/search?q=%s");
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    public void testDialogNegativeButton() {
        mCoordinator.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), any(Integer.class));
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        controller.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }
}
