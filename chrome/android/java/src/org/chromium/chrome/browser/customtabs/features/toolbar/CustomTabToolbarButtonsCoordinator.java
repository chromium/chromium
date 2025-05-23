// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.DESCRIPTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.ICON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.INDIVIDUAL_BUTTON_KEYS;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SIDE_SHEET_MAXIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.VISIBLE;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TitleVisibility;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.customtabs.CustomTabFeatureOverridesManager;
import org.chromium.chrome.browser.customtabs.features.CustomTabDimensionUtils;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabSideSheetStrategy.MaximizeButtonCallback;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CloseButtonData;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MinimizeButtonData;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SideSheetMaximizeButtonData;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
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
    private final CustomTabToolbarButtonsMediator mMediator;

    public CustomTabToolbarButtonsCoordinator(
            Activity activity,
            CustomTabToolbar view,
            BrowserServicesIntentDataProvider intentDataProvider,
            Callback<CustomButtonParams> customButtonClickCallback,
            CustomTabMinimizeDelegate minimizeDelegate,
            @Nullable CustomTabFeatureOverridesManager featureOverridesManager,
            CustomTabToolbar.@Nullable OmniboxParams omniboxParams,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        var customActionButtons =
                getCustomActionButtonsModel(
                        activity, intentDataProvider, customButtonClickCallback);
        boolean closeButtonVisible = intentDataProvider.isCloseButtonEnabled();
        int closeButtonPosition = intentDataProvider.getCloseButtonPosition();

        var closeButton =
                closeButtonVisible
                        ? getCloseButtonData(
                                closeButtonVisible,
                                intentDataProvider.getCloseButtonDrawable(),
                                closeButtonPosition)
                        : new CloseButtonData();

        int toolbarWidth = CustomTabDimensionUtils.getInitialWidth(activity, intentDataProvider);
        boolean omniboxEnabled = omniboxParams != null;
        boolean titleVisible =
                intentDataProvider.getTitleVisibilityState() == TitleVisibility.VISIBLE;
        boolean isIncognito =
                intentDataProvider.getCustomTabMode() == CustomTabProfileType.INCOGNITO;
        mModel =
                CustomTabToolbarButtonsProperties.create(
                        /* customActionButtonsVisible= */ true,
                        customActionButtons,
                        // We will fill in the actual data in the mediator.
                        new MinimizeButtonData(),
                        closeButton,
                        // TODO(crbug.com/402213312): Coordinate with MenuButtonCoordinator.
                        /* menuButtonVisible= */ true,
                        toolbarWidth,
                        omniboxEnabled,
                        titleVisible,
                        isIncognito);
        CustomTabToolbarButtonsViewBinder viewBinder = new CustomTabToolbarButtonsViewBinder();
        PropertyModelChangeProcessor.create(mModel, view, viewBinder);
        mCustomActionButtonsMcp =
                new ListModelChangeProcessor<>(
                        mModel.get(CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS),
                        view,
                        viewBinder);
        mModel.get(CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS)
                .addObserver(mCustomActionButtonsMcp);
        mMediator =
                new CustomTabToolbarButtonsMediator(
                        mModel,
                        activity,
                        minimizeDelegate,
                        intentDataProvider,
                        featureOverridesManager,
                        lifecycleDispatcher);
        view.setOnNewWidthMeasuredListener(mMediator);
    }

    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Shows the maximize button on the side sheet.
     *
     * @param isMaximized Whether the side sheet is starting as maximized.
     * @param toggleMaximize The callback to toggle the maximize state.
     */
    public void showSideSheetMaximizeButton(
            boolean isMaximized, MaximizeButtonCallback toggleMaximize) {
        assert ChromeFeatureList.sCctToolbarRefactor.isEnabled();
        var buttonData =
                new SideSheetMaximizeButtonData(/* visible= */ true, isMaximized, toggleMaximize);
        mModel.set(SIDE_SHEET_MAXIMIZE_BUTTON, buttonData);
    }

    public void removeSideSheetMaximizeButton() {
        assert ChromeFeatureList.sCctToolbarRefactor.isEnabled();
        var buttonData = new SideSheetMaximizeButtonData();
        mModel.set(SIDE_SHEET_MAXIMIZE_BUTTON, buttonData);
    }

    public void setMinimizeButtonEnabled(boolean enabled) {
        assert ChromeFeatureList.sCctToolbarRefactor.isEnabled();
        mMediator.setMinimizeButtonEnabled(enabled);
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

    private static CloseButtonData getCloseButtonData(
            boolean visible,
            Drawable icon,
            @CustomTabsIntent.CloseButtonPosition int closeButtonPosition) {
        return new CloseButtonData(visible, icon, closeButtonPosition);
    }
}
