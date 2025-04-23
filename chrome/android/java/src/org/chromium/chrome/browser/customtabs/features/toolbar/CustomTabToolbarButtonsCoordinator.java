// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.DESCRIPTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.ICON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.INDIVIDUAL_BUTTON_KEYS;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.VISIBLE;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

public class CustomTabToolbarButtonsCoordinator {
    private final ListModelChangeProcessor<
                    PropertyListModel<PropertyModel, PropertyKey>, CustomTabToolbar, PropertyKey>
            mCustomActionButtonsMcp;
    private final PropertyModel mModel;

    public CustomTabToolbarButtonsCoordinator(
            CustomTabToolbar view,
            BrowserServicesIntentDataProvider intentDataProvider,
            Callback<CustomButtonParams> customButtonClickCallback) {
        CustomTabToolbarButtonsViewBinder viewBinder = new CustomTabToolbarButtonsViewBinder();
        var customActionButtons =
                getCustomActionButtonsModel(
                        view.getContext(), intentDataProvider, customButtonClickCallback);
        mModel = CustomTabToolbarButtonsProperties.create(customActionButtons);
        PropertyModelChangeProcessor.create(mModel, view, viewBinder);
        mCustomActionButtonsMcp =
                new ListModelChangeProcessor<>(
                        mModel.get(CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS),
                        view,
                        viewBinder);
        mModel.get(CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS)
                .addObserver(mCustomActionButtonsMcp);
    }

    static PropertyListModel<PropertyModel, PropertyKey> getCustomActionButtonsModel(
            Context context,
            BrowserServicesIntentDataProvider intentDataProvider,
            Callback<CustomButtonParams> customButtonClickCallback) {
        PropertyListModel<PropertyModel, PropertyKey> listModel = new PropertyListModel<>();
        List<CustomButtonParams> customButtons = intentDataProvider.getCustomButtonsOnToolbar();
        for (var customButton : customButtons) {
            PropertyModel model =
                    new PropertyModel.Builder(INDIVIDUAL_BUTTON_KEYS)
                            .with(VISIBLE, true)
                            .with(ICON, customButton.getIcon(context))
                            .with(
                                    CLICK_LISTENER,
                                    v -> customButtonClickCallback.onResult(customButton))
                            .with(DESCRIPTION, customButton.getDescription())
                            .build();
            listModel.add(model);
        }
        return listModel;
    }
}
