// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Impl class that will resolve components for tab management.
 */
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public Layout createTabSwitcherLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, TabSwitcher tabSwitcher, ViewGroup tabSwitcherScrimAnchor,
            ScrimCoordinator scrimCoordinator) {
        return new TabSwitcherLayout(context, updateHost, renderHost, tabSwitcher,
                tabSwitcherScrimAnchor, scrimCoordinator);
    }

    @Override
    public TabSwitcher createGridTabSwitcher(@NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull ViewGroup containerView,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @Nullable BackPressManager backPressManager) {
        return new TabSwitcherCoordinator(activity, activityLifecycleDispatcher, tabModelSelector,
                tabContentManager, browserControlsStateProvider, tabCreatorManager,
                menuOrKeyboardActionController, containerView, multiWindowModeStateDispatcher,
                scrimCoordinator,
                TabUiFeatureUtilities.shouldUseListMode(activity)
                        ? TabListCoordinator.TabListMode.LIST
                        : TabListCoordinator.TabListMode.GRID,
                rootView, dynamicResourceLoaderSupplier, snackbarManager, modalDialogManager,
                incognitoReauthControllerSupplier, backPressManager);
    }

    @Override
    public TabSwitcher createCarouselTabSwitcher(@NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControls,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull ViewGroup containerView,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager) {
        return new TabSwitcherCoordinator(activity, lifecycleDispatcher, tabModelSelector,
                tabContentManager, browserControls, tabCreatorManager,
                menuOrKeyboardActionController, containerView, multiWindowModeStateDispatcher,
                scrimCoordinator, TabListCoordinator.TabListMode.CAROUSEL, rootView,
                dynamicResourceLoaderSupplier, snackbarManager, modalDialogManager, null, null);
    }

    @Override
    public TabGroupUi createTabGroupUi(@NonNull Activity activity, @NonNull ViewGroup parentView,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier, TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager, ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull SnackbarManager snackbarManager) {
        return new TabGroupUiCoordinator(activity, parentView, incognitoStateProvider,
                scrimCoordinator, omniboxFocusStateSupplier, bottomSheetController,
                activityLifecycleDispatcher, isWarmOnResumeSupplier, tabModelSelector,
                tabContentManager, rootView, dynamicResourceLoaderSupplier, tabCreatorManager,
                layoutStateProviderSupplier, snackbarManager);
    }

    @Override
    public TabGroupModelFilter createTabGroupModelFilter(TabModel tabModel) {
        return new TabGroupModelFilter(
                tabModel, TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.getValue());
    }

    @Override
    public TabSuggestions createTabSuggestions(@NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        return new TabSuggestionsOrchestrator(
                context, tabModelSelector, activityLifecycleDispatcher);
    }
}
