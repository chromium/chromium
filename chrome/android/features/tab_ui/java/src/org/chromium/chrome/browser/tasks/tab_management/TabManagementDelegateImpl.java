// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.SysUtils;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceCoordinator;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Impl class that will resolve components for tab management.
 */
@UsedByReflection("TabManagementModule")
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public TasksSurface createTasksSurface(@NonNull Activity activity,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull PropertyModel propertyModel,
            @TabSwitcherType int tabSwitcherType, @NonNull Supplier<Tab> parentTabSupplier,
            boolean hasMVTiles, @NonNull WindowAndroid windowAndroid,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector, @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ViewGroup rootView) {
        return new TasksSurfaceCoordinator(activity, scrimCoordinator, propertyModel,
                tabSwitcherType, parentTabSupplier, hasMVTiles, windowAndroid,
                activityLifecycleDispatcher, tabModelSelector, snackbarManager,
                dynamicResourceLoaderSupplier, tabContentManager, modalDialogManager,
                browserControlsStateProvider, tabCreatorManager, menuOrKeyboardActionController,
                shareDelegateSupplier, multiWindowModeStateDispatcher, rootView);
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
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView) {
        if (UmaSessionStats.isMetricsServiceAvailable()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }

        return new TabSwitcherCoordinator(activity, activityLifecycleDispatcher, tabModelSelector,
                tabContentManager, browserControlsStateProvider, tabCreatorManager,
                menuOrKeyboardActionController, containerView, shareDelegateSupplier,
                multiWindowModeStateDispatcher, scrimCoordinator,
                TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(activity)
                                && SysUtils.isLowEndDevice()
                        ? TabListCoordinator.TabListMode.LIST
                        : TabListCoordinator.TabListMode.GRID,
                rootView);
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
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView) {
        return new TabSwitcherCoordinator(activity, lifecycleDispatcher, tabModelSelector,
                tabContentManager, browserControls, tabCreatorManager,
                menuOrKeyboardActionController, containerView, shareDelegateSupplier,
                multiWindowModeStateDispatcher, scrimCoordinator,
                TabListCoordinator.TabListMode.CAROUSEL, rootView);
    }

    @Override
    public TabGroupUi createTabGroupUi(@NonNull Activity activity, @NonNull ViewGroup parentView,
            @NonNull ThemeColorProvider themeColorProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier, TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager, ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            @NonNull SnackbarManager snackbarManager) {
        return new TabGroupUiCoordinator(activity, parentView, themeColorProvider, scrimCoordinator,
                omniboxFocusStateSupplier, bottomSheetController, activityLifecycleDispatcher,
                isWarmOnResumeSupplier, tabModelSelector, tabContentManager, rootView,
                dynamicResourceLoaderSupplier, tabCreatorManager, shareDelegateSupplier,
                overviewModeBehaviorSupplier, snackbarManager);
    }

    @Override
    public Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface, JankTracker jankTracker) {
        return StartSurfaceDelegate.createStartSurfaceLayout(
                context, updateHost, renderHost, startSurface, jankTracker);
    }

    @Override
    public StartSurface createStartSurface(@NonNull Activity activity,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull BottomSheetController sheetController,
            @NonNull OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            @NonNull Supplier<Tab> parentTabSupplier, boolean hadWarmStart,
            @NonNull WindowAndroid windowAndroid, @NonNull ViewGroup containerView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull Supplier<OmniboxStub> omniboxStubSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull JankTracker jankTracker) {
        return StartSurfaceDelegate.createStartSurface(activity, scrimCoordinator, sheetController,
                startSurfaceOneshotSupplier, parentTabSupplier, hadWarmStart, windowAndroid,
                containerView, dynamicResourceLoaderSupplier, tabModelSelector,
                browserControlsManager, snackbarManager, shareDelegateSupplier, omniboxStubSupplier,
                tabContentManager, modalDialogManager, chromeActivityNativeDelegate,
                activityLifecycleDispatcher, tabCreatorManager, menuOrKeyboardActionController,
                multiWindowModeStateDispatcher, jankTracker);
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

    @Override
    public void applyThemeOverlays(Activity activity) {
        activity.setTheme(TabUiThemeProvider.getThemeOverlayStyleResourceId());
    }
}
