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
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestions;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
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
     * Create the {@link TasksSurface}
     * @param activity The {@link Activity} that creates this surface.
     * @param scrimCoordinator The {@link ScrimCoordinator} that controls scrim view.
     * @param propertyModel The {@link PropertyModel} contains the {@link TasksSurfaceProperties}
     *         to communicate with this surface.
     * @param tabSwitcherType The type of the tab switcher to show.
     * @param parentTabSupplier {@link Supplier} to provide parent tab for the
     *         TasksSurface.
     * @param hasMVTiles whether has MV tiles on the surface.
     * @param windowAndroid An instance of a {@link WindowAndroid}.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param snackbarManager Manages the display of snackbars.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param tabContentManager Gives access to the tab content.
     * @param modalDialogManager Manages the display of modal dialogs.
     * @param browserControlsStateProvider Gives access to the state of the browser controls.
     * @param tabCreatorManger Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param shareDelegateSupplier Supplies the current {@link ShareDelegate}.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param rootView The root view of the app.
     * @return The {@link TasksSurface}.
     */
    TasksSurface createTasksSurface(@NonNull Activity activity,
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
            @NonNull ViewGroup rootView);

    /**
     * Create the {@link TabSwitcher} to display Tabs in grid.
     * @param activity The current android {@link Activity}.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param browserControlsStateProvider Gives access to the state of the browser controls.
     * @param tabCreatorManger Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @param shareDelegateSupplier Supplies the current {@link ShareDelegate}.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @param rootView The root view of the app.
     * @return The {@link TabSwitcher}.
     */
    TabSwitcher createGridTabSwitcher(@NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull ViewGroup containerView,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView);

    /**
     * Create the {@link TabSwitcher} to display Tabs in carousel.
     * @param activity The current Android {@link Activity}.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param browserControls Allows observation of the browser controls state.
     * @param tabCreatorManger Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @param shareDelegateSupplier Supplies the current {@link ShareDelegate}.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @param rootView The root view of the app.
     * @return The {@link TabSwitcher}.
     */
    TabSwitcher createCarouselTabSwitcher(@NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControls,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull ViewGroup containerView,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView);

    /**
     * Create the {@link TabGroupUi}.
     * @param activity The {@link Activity} that creates this surface.
     * @param parentView The parent view of this UI.
     * @param themeColorProvider The {@link ThemeColorProvider} for this UI.
     * @param scrimCoordinator   The {@link ScrimCoordinator} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @param bottomSheetController Controls the state of the bottom sheet.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param isWarmOnResumeSupplier Supplies whether the app was warm on resume.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param rootView The root view of the app.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param tabCreatorManger Manages creation of tabs.
     * @param shareDelegateSupplier Supplies the current {@link ShareDelegate}.
     * @param overviewModeBehaviorSupplier Suppolies the current {@link OverviewModeBehavior}.
     * @param snackbarManager Manages the display of snackbars.
     * @return The {@link TabGroupUi}.
     */
    TabGroupUi createTabGroupUi(@NonNull Activity activity, @NonNull ViewGroup parentView,
            @NonNull ThemeColorProvider themeColorProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager, @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            @NonNull SnackbarManager snackbarManager);

    /**
     * Create the {@link StartSurfaceLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param startSurface The {@link StartSurface} the layout should own.
     * @return The {@link StartSurfaceLayout}.
     */
    Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface, JankTracker jankTracker);

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
     * @param omniboxStubSupplier Supplies the {@link OmniboxStub}.
     * @param tabContentManager Gives access to the tab content.
     * @param modalDialogManager Manages the display of modal dialogs.
     * @param chromeActivityNativeDelegate Delegate for native initialization.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabCreatorManger Manages creation of tabs.
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
            @NonNull Supplier<OmniboxStub> omniboxStubSupplier,
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
     * Create a {@link TabSuggestions} for the given {@link Activity}
     * @param context The activity context.
     * @param tabModelSelector Allows access to the current set of {@link TabModel}.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @return the {@link TabSuggestions} for the activity
     */
    TabSuggestions createTabSuggestions(@NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher);

    /**
     * Apply the theme overlay for the target activity used for Tab management components. This
     * theme needs to be applied once before creating any of the tab related component.
     * @param activity The target {@link Activity} that used Tab theme.
     */
    void applyThemeOverlays(@NonNull Activity activity);
}
