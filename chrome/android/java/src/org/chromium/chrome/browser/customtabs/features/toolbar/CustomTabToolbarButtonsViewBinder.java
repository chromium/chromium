// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.DESCRIPTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.ICON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SIDE_SHEET_MAXIMIZE_BUTTON;

import android.support.annotation.DrawableRes;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SideSheetMaximizeButtonData;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class CustomTabToolbarButtonsViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                        PropertyModel, CustomTabToolbar, PropertyKey>,
                ListModelChangeProcessor.ViewBinder<
                        PropertyListModel<PropertyModel, PropertyKey>,
                        CustomTabToolbar,
                        PropertyKey> {
    @Override
    public void bind(PropertyModel model, CustomTabToolbar view, PropertyKey propertyKey) {
        if (propertyKey == CUSTOM_ACTION_BUTTONS) {
            view.setCustomActionButtonsListModel(model.get(CUSTOM_ACTION_BUTTONS));
        } else if (propertyKey == SIDE_SHEET_MAXIMIZE_BUTTON) {
            prepareSideSheetMaximizeButton(view, model.get(SIDE_SHEET_MAXIMIZE_BUTTON));
            view.reinflateAndRepositionToolbarElements();
        }
    }

    @Override
    public void onItemsInserted(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count) {
        view.reinflateAndRepositionToolbarElements();
    }

    @Override
    public void onItemsRemoved(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count) {
        view.reinflateAndRepositionToolbarElements();
    }

    @Override
    public void onItemsChanged(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count,
            @Nullable PropertyKey payload) {
        for (int i = index; i < index + count; i++) {
            PropertyModel customButtonModel = model.get(i);
            view.updateCustomActionButton(
                    index, customButtonModel.get(ICON), customButtonModel.get(DESCRIPTION));
        }
    }

    private static void prepareSideSheetMaximizeButton(
            CustomTabToolbar view, SideSheetMaximizeButtonData data) {
        ImageButton button = view.findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (button == null && data.visible) {
            LayoutInflater.from(view.getContext())
                    .inflate(R.layout.custom_tabs_sidepanel_maximize, view, true);
            button = view.findViewById(R.id.custom_tabs_sidepanel_maximize);
        }

        if (button == null) return;
        if (!data.visible) {
            button.setVisibility(View.GONE);
            return;
        }

        button.setVisibility(View.VISIBLE);
        boolean maximized = data.maximized;
        var callback = data.callback;
        button.setOnClickListener(
                v -> setSideSheetMaximizeButtonDrawable((ImageButton) v, callback.onClick()));
        setSideSheetMaximizeButtonDrawable(button, maximized);
    }

    private static void setSideSheetMaximizeButtonDrawable(ImageButton button, boolean maximized) {
        @DrawableRes
        int drawableId = maximized ? R.drawable.ic_fullscreen_exit : R.drawable.ic_fullscreen_enter;
        int buttonDescId =
                maximized
                        ? R.string.custom_tab_side_sheet_minimize
                        : R.string.custom_tab_side_sheet_maximize;
        var drawable =
                UiUtils.getTintedDrawable(
                        button.getContext(),
                        drawableId,
                        ChromeColors.getPrimaryIconTint(button.getContext(), false));
        button.setImageDrawable(drawable);
        button.setContentDescription(button.getContext().getString(buttonDescId));
    }
}
