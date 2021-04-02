// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.SysUtils;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceCoordinator;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Impl class that will resolve components for tab management.
 */
@UsedByReflection("TabManagementModule")
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public TasksSurface createTasksSurface(ChromeActivity activity,
            ScrimCoordinator scrimCoordinator, PropertyModel propertyModel,
            @TabSwitcherType int tabSwitcherType, Supplier<Tab> parentTabSupplier,
            boolean hasMVTiles, boolean hasTrendyTerms, WindowAndroid windowAndroid) {
        return new TasksSurfaceCoordinator(activity, scrimCoordinator, propertyModel,
                tabSwitcherType, parentTabSupplier, hasMVTiles, hasTrendyTerms, windowAndroid);
    }

    @Override
    public TabSwitcher createGridTabSwitcher(
            ChromeActivity activity, ViewGroup containerView, ScrimCoordinator scrimCoordinator) {
        if (UmaSessionStats.isMetricsServiceAvailable()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }

        return new TabSwitcherCoordinator(activity, activity.getLifecycleDispatcher(),
                activity.getTabModelSelector(), activity.getTabContentManager(),
                activity.getBrowserControlsManager(), activity,
                activity.getMenuOrKeyboardActionController(), containerView,
                activity.getShareDelegateSupplier(), activity.getMultiWindowModeStateDispatcher(),
                scrimCoordinator,
                TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()
                                && SysUtils.isLowEndDevice()
                        ? TabListCoordinator.TabListMode.LIST
                        : TabListCoordinator.TabListMode.GRID);
    }

    @Override
    public TabSwitcher createCarouselTabSwitcher(
            ChromeActivity activity, ViewGroup containerView, ScrimCoordinator scrimCoordinator) {
        return new TabSwitcherCoordinator(activity, activity.getLifecycleDispatcher(),
                activity.getTabModelSelector(), activity.getTabContentManager(),
                activity.getBrowserControlsManager(), activity,
                activity.getMenuOrKeyboardActionController(), containerView,
                activity.getShareDelegateSupplier(), activity.getMultiWindowModeStateDispatcher(),
                scrimCoordinator, TabListCoordinator.TabListMode.CAROUSEL);
    }

    @Override
    public TabGroupUi createTabGroupUi(ViewGroup parentView, ThemeColorProvider themeColorProvider,
            ScrimCoordinator scrimCoordinator,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        return new TabGroupUiCoordinator(
                parentView, themeColorProvider, scrimCoordinator, omniboxFocusStateSupplier);
    }

    @Override
    public Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface) {
        return StartSurfaceDelegate.createStartSurfaceLayout(
                context, updateHost, renderHost, startSurface);
    }

    @Override
    public StartSurface createStartSurface(ChromeActivity activity,
            ScrimCoordinator scrimCoordinator, BottomSheetController sheetController,
            OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            Supplier<Tab> parentTabSupplier, boolean hadWarmStart, WindowAndroid windowAndroid) {
        return StartSurfaceDelegate.createStartSurface(activity, scrimCoordinator, sheetController,
                startSurfaceOneshotSupplier, parentTabSupplier, hadWarmStart, windowAndroid);
    }

    @Override
    public TabGroupModelFilter createTabGroupModelFilter(TabModel tabModel) {
        return new TabGroupModelFilter(
                tabModel, TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.getValue());
    }

    @Override
    public TabSuggestions createTabSuggestions(ChromeActivity activity) {
        return new TabSuggestionsOrchestrator(
                activity.getTabModelSelector(), activity.getLifecycleDispatcher());
    }
}
