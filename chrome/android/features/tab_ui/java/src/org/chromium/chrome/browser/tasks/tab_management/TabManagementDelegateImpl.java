// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceCoordinator;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceCoordinator;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Impl class that will resolve components for tab management.
 */
@UsedByReflection("TabManagementModule")
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public TasksSurface createTasksSurface(ChromeActivity activity, PropertyModel propertyModel,
            FakeboxDelegate fakeboxDelegate, boolean isTabCarousel) {
        return new TasksSurfaceCoordinator(activity, propertyModel, fakeboxDelegate, isTabCarousel);
    }

    @Override
    public TabSwitcher createGridTabSwitcher(ChromeActivity activity, ViewGroup containerView) {
        if (UmaSessionStats.isMetricsServiceAvailable()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }
        return new TabSwitcherCoordinator(activity, activity.getLifecycleDispatcher(),
                activity.getTabModelSelector(), activity.getTabContentManager(),
                activity.getCompositorViewHolder().getDynamicResourceLoader(),
                activity.getFullscreenManager(), activity,
                activity.getMenuOrKeyboardActionController(), activity, containerView,
                TabListCoordinator.TabListMode.GRID);
    }

    @Override
    public TabSwitcher createCarouselTabSwitcher(ChromeActivity activity, ViewGroup containerView) {
        return new TabSwitcherCoordinator(activity, activity.getLifecycleDispatcher(),
                activity.getTabModelSelector(), activity.getTabContentManager(),
                activity.getCompositorViewHolder().getDynamicResourceLoader(),
                activity.getFullscreenManager(), activity,
                activity.getMenuOrKeyboardActionController(), activity, containerView,
                TabListCoordinator.TabListMode.CAROUSEL);
    }

    @Override
    public TabGroupUi createTabGroupUi(
            ViewGroup parentView, ThemeColorProvider themeColorProvider) {
        return new TabGroupUiCoordinator(parentView, themeColorProvider);
    }

    @Override
    public Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface) {
        return new StartSurfaceLayout(context, updateHost, renderHost, startSurface);
    }

    @Override
    public StartSurface createStartSurface(ChromeActivity activity) {
        return new StartSurfaceCoordinator(activity);
    }

    @Override
    public TabGroupModelFilter createTabGroupModelFilter(TabModel tabModel) {
        return new TabGroupModelFilter(tabModel);
    }

    @Override
    public TabSuggestions createTabSuggestions(ChromeActivity activity) {
        return new TabSuggestionsOrchestrator(
                activity.getTabModelSelector(), activity.getLifecycleDispatcher());
    }
}
