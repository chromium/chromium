// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.DESCRIPTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.ICON;

import org.chromium.build.annotations.Nullable;
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
}
