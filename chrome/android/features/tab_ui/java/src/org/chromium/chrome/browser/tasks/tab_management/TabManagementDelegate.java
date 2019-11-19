// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Interface to get access to components concerning tab management.
 * TODO(crbug.com/982018): Move DFM configurations to 'chrome/android/modules/start_surface/'
 */
@ModuleInterface(module = "tab_management",
        impl = "org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateImpl")
public interface TabManagementDelegate {
    /**
     * Create the {@link TasksSurface}
     * @param activity The {@link ChromeActivity} that creates this surface.
     * @param propertyModel The {@link PropertyModel} contains the {@link TasksSurfaceProperties} to
     *         communicate with this surface.
     * @param fakeboxDelegate The delegate of the fake search box.
     * @param isTabCarousel Whether show the Tabs in carousel mode.
     * @return The {@link TasksSurface}.
     */
    TasksSurface createTasksSurface(ChromeActivity activity, PropertyModel propertyModel,
            FakeboxDelegate fakeboxDelegate, boolean isTabCarousel);

    /**
     * Create the {@link TabSwitcher} to display Tabs in grid.
     * @param context The {@link Context} of this switcher.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @return The {@link TabSwitcher}.
     */
    TabSwitcher createGridTabSwitcher(ChromeActivity context, ViewGroup containerView);

    /**
     * Create the {@link TabSwitcher} to display Tabs in carousel.
     * @param context The {@link Context} of this switcher.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @return The {@link TabSwitcher}.
     */
    TabSwitcher createCarouselTabSwitcher(ChromeActivity context, ViewGroup containerView);

    /**
     * Create the {@link TabGroupUi}.
     * @param parentView The parent view of this UI.
     * @param themeColorProvider The {@link ThemeColorProvider} for this UI.
     * @return The {@link TabGroupUi}.
     */
    TabGroupUi createTabGroupUi(ViewGroup parentView, ThemeColorProvider themeColorProvider);

    /**
     * Create the {@link StartSurfaceLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param startSurface The {@link StartSurface} the layout should own.
     * @return The {@link StartSurfaceLayout}.
     */
    Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface);

    /**
     * Create the {@link StartSurface}
     * @param activity The {@link ChromeActivity} creates this {@link StartSurface}.
     * @return the {@link StartSurface}
     */
    StartSurface createStartSurface(ChromeActivity activity);

    /**
     * Create a {@link TabGroupModelFilter} for the given {@link TabModel}.
     * @return The {@link TabGroupModelFilter}.
     */
    TabGroupModelFilter createTabGroupModelFilter(TabModel tabModel);

    /**
     * Create a {@link TabSuggestions} for the given {@link ChromeActivity}
     * @param activity the {@link ChromeActivity} creates this {@link TabSuggestions}.
     * @return the {@link TabSuggestions} for the activity
     */
    TabSuggestions createTabSuggestions(ChromeActivity activity);
}
