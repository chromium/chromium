// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.jank_tracker.JankTracker;
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
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.module_installer.builder.ModuleInterface;
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
     * Create the {@link TabSwitcherLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param tabSwitcher The {@link TabSwitcher} the layout should own.
     * @param tabSwitcherScrimAnchor {@link ViewGroup} used by tab switcher layout to show scrim
     *         when overview is visible.
     * @param scrimCoordinator {@link ScrimCoordinator} to show/hide scrim.
     * @return The {@link TabSwitcherLayout}.
     */
    Layout createTabSwitcherLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, TabSwitcher tabSwitcher, JankTracker jankTracker,
            ViewGroup tabSwitcherScrimAnchor, ScrimCoordinator scrimCoordinator);

    /**
     * Create the {@link TabSwitcher} to display Tabs in grid.
     * @param activity The current android {@link Activity}.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param browserControlsStateProvider Gives access to the state of the browser controls.
     * @param tabCreatorManager Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @param rootView The root view of the app.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param snackbarManager Manages the snackbar.
     * @param modalDialogManager Manages modal dialogs.
     * @param incognitoReauthControllerSupplier {@link OneshotSupplier<IncognitoReauthController>}
     *         to detect pending re-auth when tab switcher is shown.
     * @param backPressManager {@link BackPressManager} to handle back press gesture.
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
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @Nullable BackPressManager backPressManager);

    /**
     * Create the {@link TabSwitcher} to display Tabs in carousel.
     * @param activity The current Android {@link Activity}.
     * @param lifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param browserControls Allows observation of the browser controls state.
     * @param tabCreatorManager Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param containerView The {@link ViewGroup} to add the switcher to.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control the scrim view.
     * @param rootView The root view of the app.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param snackbarManager Manages the snackbar.
     * @param modalDialogManager Manages modal dialogs.
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
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager);

    /**
     * Create the {@link TabGroupUi}.
     * @param activity The {@link Activity} that creates this surface.
     * @param parentView The parent view of this UI.
     * @param incognitoStateProvider Observable provider of incognito state.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @param bottomSheetController Controls the state of the bottom sheet.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param isWarmOnResumeSupplier Supplies whether the app was warm on resume.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param rootView The root view of the app.
     * @param dynamicResourceLoaderSupplier Supplies the current {@link DynamicResourceLoader}.
     * @param tabCreatorManager Manages creation of tabs.
     * @param layoutStateProviderSupplier Supplies the {@link LayoutStateProvider}.
     * @param snackbarManager Manages the display of snackbars.
     * @return The {@link TabGroupUi}.
     */
    TabGroupUi createTabGroupUi(@NonNull Activity activity, @NonNull ViewGroup parentView,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager, @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull SnackbarManager snackbarManager);

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
}
