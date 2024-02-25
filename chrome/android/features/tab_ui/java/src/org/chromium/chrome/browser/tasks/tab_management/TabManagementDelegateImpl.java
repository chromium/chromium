// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.util.Pair;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;
import java.util.function.DoubleConsumer;

/** Impl class that will resolve components for tab management. */
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public Layout createTabSwitcherLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutStateProvider layoutStateProvider,
            LayoutRenderHost renderHost,
            BrowserControlsStateProvider browserControlsStateProvider,
            TabSwitcher tabSwitcher,
            ViewGroup tabSwitcherScrimAnchor,
            ScrimCoordinator scrimCoordinator) {
        return new TabSwitcherLayout(
                context,
                updateHost,
                layoutStateProvider,
                renderHost,
                browserControlsStateProvider,
                tabSwitcher,
                tabSwitcherScrimAnchor,
                scrimCoordinator);
    }

    @Override
    public TabSwitcher createGridTabSwitcher(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull ViewGroup containerView,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @Nullable BackPressManager backPressManager,
            @Nullable OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier) {
        return new TabSwitcherCoordinator(
                activity,
                activityLifecycleDispatcher,
                tabModelSelector,
                tabContentManager,
                browserControlsStateProvider,
                tabCreatorManager,
                menuOrKeyboardActionController,
                containerView,
                multiWindowModeStateDispatcher,
                scrimCoordinator,
                TabUiFeatureUtilities.shouldUseListMode()
                        ? TabListCoordinator.TabListMode.LIST
                        : TabListCoordinator.TabListMode.GRID,
                rootView,
                dynamicResourceLoaderSupplier,
                snackbarManager,
                modalDialogManager,
                incognitoReauthControllerSupplier,
                backPressManager,
                layoutStateProviderSupplier);
    }

    @Override
    public TabGroupUi createTabGroupUi(
            @NonNull Activity activity,
            @NonNull ViewGroup parentView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull SnackbarManager snackbarManager) {
        return new TabGroupUiCoordinator(
                activity,
                parentView,
                browserControlsStateProvider,
                incognitoStateProvider,
                scrimCoordinator,
                omniboxFocusStateSupplier,
                bottomSheetController,
                activityLifecycleDispatcher,
                isWarmOnResumeSupplier,
                tabModelSelector,
                tabContentManager,
                rootView,
                dynamicResourceLoaderSupplier,
                tabCreatorManager,
                layoutStateProviderSupplier,
                snackbarManager);
    }

    @Override
    public Pair<TabSwitcher, Pane> createTabSwitcherPane(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator rootUiScrimCoordinator,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @NonNull OnClickListener newTabButtonOnClickListener,
            boolean isIncognito,
            @NonNull DoubleConsumer onToolbarAlphaChange) {
        // TODO(crbug/1505772): Consider making this an activity scoped singleton and possibly
        // hosting it in CTA/HubProvider.
        TabSwitcherPaneCoordinatorFactory factory =
                new TabSwitcherPaneCoordinatorFactory(
                        activity,
                        lifecycleDispatcher,
                        profileProviderSupplier,
                        tabModelSelector,
                        tabContentManager,
                        tabCreatorManager,
                        browserControlsStateProvider,
                        multiWindowModeStateDispatcher,
                        rootUiScrimCoordinator,
                        snackbarManager,
                        modalDialogManager);
        TabSwitcherPaneBase pane;
        if (isIncognito) {
            Supplier<TabModelFilter> incongitorTabModelFilterSupplier =
                    () -> tabModelSelector.getTabModelFilterProvider().getTabModelFilter(true);
            pane =
                    new IncognitoTabSwitcherPane(
                            activity,
                            factory,
                            incongitorTabModelFilterSupplier,
                            newTabButtonOnClickListener,
                            incognitoReauthControllerSupplier,
                            onToolbarAlphaChange);
        } else {
            Supplier<TabModelFilter> tabModelFilterSupplier =
                    () -> tabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
            pane =
                    new TabSwitcherPane(
                            activity,
                            ContextUtils.getAppSharedPreferences(),
                            profileProviderSupplier,
                            factory,
                            tabModelFilterSupplier,
                            newTabButtonOnClickListener,
                            new TabSwitcherPaneDrawableCoordinator(activity, tabModelSelector),
                            onToolbarAlphaChange);
        }
        return Pair.create(new TabSwitcherPaneAdapter(pane), pane);
    }

    @Override
    public Pane createTabGroupsPane(
            @NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull DoubleConsumer onToolbarAlphaChange) {
        LazyOneshotSupplier<TabModelFilter> tabModelFilterSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () ->
                                tabModelSelector
                                        .getTabModelFilterProvider()
                                        .getTabModelFilter(false));
        return new TabGroupsPane(context, tabModelFilterSupplier, onToolbarAlphaChange);
    }

    @Override
    public TabGroupCreationDialog createTabGroupCreationDialogDelegate(
            @NonNull Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        return new TabGroupCreationDialogDelegate(
                activity, modalDialogManager, tabModelSelectorSupplier);
    }

    @Override
    public ColorPicker createColorPickerCoordinator(
            @NonNull Context context,
            @NonNull List<Integer> colors,
            @NonNull @LayoutRes int colorPickerLayout,
            @NonNull @ColorPickerType int colorPickerType,
            @NonNull boolean isIncognito) {
        return new ColorPickerCoordinator(
                context, colors, colorPickerLayout, colorPickerType, isIncognito);
    }
}
