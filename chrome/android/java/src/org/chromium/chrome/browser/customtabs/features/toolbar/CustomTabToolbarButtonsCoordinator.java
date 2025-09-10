// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CLOSE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.DESCRIPTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.ICON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.INDIVIDUAL_BUTTON_KEYS;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SIDE_SHEET_MAXIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TYPE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.VISIBLE;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TitleVisibility;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.customtabs.features.CustomTabDimensionUtils;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabSideSheetStrategy.MaximizeButtonCallback;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CloseButtonData;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MinimizeButtonData;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SideSheetMaximizeButtonData;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.top.OptionalBrowsingModeButtonController;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;
import java.util.function.Supplier;

@NullMarked
public class CustomTabToolbarButtonsCoordinator
        implements MenuButtonCoordinator.VisibilityDelegate,
                OptionalBrowsingModeButtonController.Delegate {
    private final ListModelChangeProcessor<
                    PropertyListModel<PropertyModel, PropertyKey>, CustomTabToolbar, PropertyKey>
            mCustomActionButtonsMcp;
    private final PropertyModel mModel;
    private final CustomTabToolbarButtonsMediator mMediator;
    private final boolean mIsOptionalButtonSupported;

    public CustomTabToolbarButtonsCoordinator(
            Activity activity,
            CustomTabToolbar view,
            BrowserServicesIntentDataProvider intentDataProvider,
            Callback<CustomButtonParams> customButtonClickCallback,
            CustomTabMinimizeDelegate minimizeDelegate,
            Supplier<AppMenuHandler> appMenuHandler,
            CustomTabToolbar.@Nullable OmniboxParams omniboxParams,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider tabProvider) {
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
                                closeButtonPosition,
                                /* clickListener= */ v -> {}) // Real value set later by
                        // #setCloseButtonClickHandler.
                        : new CloseButtonData();

        mIsOptionalButtonSupported = intentDataProvider.isOptionalButtonSupported();
        int toolbarWidth = CustomTabDimensionUtils.getInitialWidth(activity, intentDataProvider);
        boolean omniboxEnabled = omniboxParams != null;
        boolean titleVisible =
                intentDataProvider.getTitleVisibilityState() == TitleVisibility.VISIBLE;
        boolean isIncognito =
                intentDataProvider.getCustomTabMode() == CustomTabProfileType.INCOGNITO;
        var tint = ChromeColors.getPrimaryIconTint(activity, isIncognito);
        mModel =
                CustomTabToolbarButtonsProperties.create(
                        /* customActionButtonsVisible= */ true,
                        customActionButtons,
                        // We will fill in the actual data in the mediator.
                        new MinimizeButtonData(),
                        closeButton,
                        /* menuButtonVisible= */ true,
                        /* optionalButtonVisible= */ false,
                        toolbarWidth,
                        omniboxEnabled,
                        titleVisible,
                        isIncognito,
                        tint);
        view.setTag(R.id.view_model, mModel);
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
                        view,
                        activity,
                        minimizeDelegate,
                        intentDataProvider,
                        lifecycleDispatcher,
                        tabProvider);
        view.setOnNewWidthMeasuredListener(mMediator);
        view.setOnColorSchemeChangedObserver(mMediator);
        view.setAppMenuHandler(appMenuHandler);
    }

    public void destroy() {
        mMediator.destroy();
    }

    @Override
    public void setMenuButtonVisible(boolean visible) {
        mModel.set(CustomTabToolbarButtonsProperties.MENU_BUTTON_VISIBLE, visible);
    }

    @Override
    public boolean isMenuButtonVisible() {
        return mModel.get(CustomTabToolbarButtonsProperties.MENU_BUTTON_VISIBLE);
    }

    @Override
    public void setOptionalButtonData(@Nullable ButtonData buttonData) {
        if (mIsOptionalButtonSupported) mMediator.setOptionalButtonData(buttonData);
    }

    @Override
    public boolean isOptionalButtonVisible() {
        return mIsOptionalButtonSupported ? mMediator.isOptionalButtonVisible() : false;
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

    public void setCloseButtonVisible(boolean visible) {
        var oldData = mModel.get(CLOSE_BUTTON);
        mModel.set(
                CLOSE_BUTTON,
                getCloseButtonData(visible, oldData.icon, oldData.position, oldData.clickListener));
    }

    public void setCloseButtonClickHandler(View.OnClickListener listener) {
        var oldData = mModel.get(CLOSE_BUTTON);
        mModel.set(
                CLOSE_BUTTON,
                getCloseButtonData(oldData.visible, oldData.icon, oldData.position, listener));
    }

    public void setCustomActionButtonsVisible(boolean visible) {
        mModel.set(CUSTOM_ACTION_BUTTONS_VISIBLE, visible);
    }

    /**
     * Updates the visual appearance of a custom action button.
     *
     * @param index The index of the button.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        mMediator.updateCustomActionButton(index, drawable, description);
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
                            .with(TYPE, customButton.getType())
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
            @Nullable Drawable icon,
            @CustomTabsIntent.CloseButtonPosition int closeButtonPosition,
            View.OnClickListener clickListener) {
        return new CloseButtonData(visible, icon, closeButtonPosition, clickListener);
    }
}
