// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

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
     * Create the {@link StartSurfaceLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param startSurface The {@link StartSurface} the layout should own.
     * @param startSurfaceScrimAnchor {@link ViewGroup} used by start surface layout to show scrim
     *         when overview is visible.
     * @param scrimCoordinator {@link ScrimCoordinator} to show/hide scrim.
     * @return The {@link StartSurfaceLayout}.
     */
    Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface, JankTracker jankTracker,
            ScrimCoordinator scrimCoordinator);

    /**
     * Create the {@link StartSurface}
     * @param activity The {@link Activity} creates this {@link StartSurface}.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @param sheetController A {@link BottomSheetController} to show content in the bottom sheet.
     * @param startSurfaceOneshotSupplier Supplies the {@link StartSurface}, passing the owned
     *         supplier to StartSurface itself.
     * @param parentTabSupplier A {@link Supplier} to provide parent tab for
     *         StartSurface.
     * @param hadWarmStart Whether the activity had a warm start because the native library was
     *         already fully loaded and initialized
     * @param windowAndroid An instance of a {@link WindowAndroid}
     * @param containerView The container {@link ViewGroup} for this ui, also the root view for
     *         StartSurface.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param browserControlsManager Manages the browser controls.
     * @param snackbarManager Manages the display of snackbars.
     * @param shareDelegateSupplier Supplies the current {@link ShareDelegate}.
     * @param tabContentManager Gives access to the tab content.
     * @param modalDialogManager Manages the display of modal dialogs.
     * @param chromeActivityNativeDelegate Delegate for native initialization.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabCreatorManager Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param jankTracker Measures jank while tab switcher is visible.
     * @return the {@link StartSurface}
     */
    StartSurface createStartSurface(@NonNull Activity activity,
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
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull JankTracker jankTracker);

    /**
     * Create a {@link TabGroupModelFilter} for the given {@link TabModel}.
     * @return The {@link TabGroupModelFilter}.
     */
    TabGroupModelFilter createTabGroupModelFilter(TabModel tabModel);

    /**
     * Apply the theme overlay for the target activity used for Tab management components. This
     * theme needs to be applied once before creating any of the tab related component.
     * @param activity The target {@link Activity} that used Tab theme.
     */
    void applyThemeOverlays(@NonNull Activity activity);
}
