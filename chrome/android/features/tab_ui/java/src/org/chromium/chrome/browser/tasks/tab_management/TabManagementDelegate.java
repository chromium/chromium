// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface to get access to components concerning tab management.
 * TODO(crbug.com/982018): Move DFM configurations to 'chrome/android/modules/start_surface/'
 */
@ModuleInterface(module = "tab_management",
        impl = "org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateImpl")
public interface TabManagementDelegate {
    @IntDef({TabSwitcherType.GRID, TabSwitcherType.CAROUSEL, TabSwitcherType.SINGLE,
            TabSwitcherType.NONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabSwitcherType {
        int GRID = 0;
        int CAROUSEL = 1;
        int SINGLE = 2;
        int NONE = 3;
    }

    /**
     * Create the {@link TasksSurface}
     * @param activity The {@link ChromeActivity} that creates this surface.
     * @param scrimCoordinator The {@link ScrimCoordinator} that controls scrim view.
     * @param propertyModel The {@link PropertyModel} contains the {@link TasksSurfaceProperties} to
     *         communicate with this surface.
     * @param tabSwitcherType The type of the tab switcher to show.
     * @param parentTabSupplier {@link Supplier} to provide parent tab for the
     *         TasksSurface.
     * @param hasMVTiles whether has MV tiles on the surface.
     * @param hasTrendyTerms whether has trendy terms on the surface.
     * @param windowAndroid An instance of a {@link WindowAndroid}
     * @return The {@link TasksSurface}.
     */
    TasksSurface createTasksSurface(ChromeActivity activity, ScrimCoordinator scrimCoordinator,
            PropertyModel propertyModel, @TabSwitcherType int tabSwitcherType,
            Supplier<Tab> parentTabSupplier, boolean hasMVTiles, boolean hasTrendyTerms,
            WindowAndroid windowAndroid);

    /**
     * Create the {@link TabSwitcher} to display Tabs in grid.
     * @param context The {@link Context} of this switcher.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @return The {@link TabSwitcher}.
     */
    TabSwitcher createGridTabSwitcher(
            ChromeActivity context, ViewGroup containerView, ScrimCoordinator scrimCoordinator);

    /**
     * Create the {@link TabSwitcher} to display Tabs in carousel.
     * @param context The {@link Context} of this switcher.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @return The {@link TabSwitcher}.
     */
    TabSwitcher createCarouselTabSwitcher(
            ChromeActivity context, ViewGroup containerView, ScrimCoordinator scrimCoordinator);

    /**
     * Create the {@link TabGroupUi}.
     * @param parentView The parent view of this UI.
     * @param themeColorProvider The {@link ThemeColorProvider} for this UI.
     * @param scrimCoordinator   The {@link ScrimCoordinator} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @return The {@link TabGroupUi}.
     */
    TabGroupUi createTabGroupUi(ViewGroup parentView, ThemeColorProvider themeColorProvider,
            ScrimCoordinator scrimCoordinator,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier);

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
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @param sheetController A {@link BottomSheetController} to show content in the bottom sheet.
     * @param parentTabSupplier A {@link Supplier} to provide parent tab for
     *         StartSurface.
     * @param hadWarmStart Whether the activity had a warm start because the native library was
     *         already fully loaded and initialized
     * @param windowAndroid An instance of a {@link WindowAndroid}
     * @return the {@link StartSurface}
     */
    StartSurface createStartSurface(ChromeActivity activity, ScrimCoordinator scrimCoordinator,
            BottomSheetController sheetController,
            OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            Supplier<Tab> parentTabSupplier, boolean hadWarmStart, WindowAndroid windowAndroid);

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
