// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.content.Context;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.crow.CrowButtonDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/** StartSurfaceDelegate. */
public class StartSurfaceDelegate {
    /**
     * Create the {@link StartSurfaceHomeLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param startSurface The {@link StartSurface} the layout should own.
     * @return The {@link StartSurfaceHomeLayout}.
     */
    public static StartSurfaceHomeLayout createStartSurfaceHomeLayout(Context context,
            LayoutUpdateHost updateHost, LayoutRenderHost renderHost, StartSurface startSurface) {
        return new StartSurfaceHomeLayout(context, updateHost, renderHost, startSurface);
    }

    /**
     * Create the {@link TabSwitcherAndStartSurfaceLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param startSurface The {@link StartSurface} the layout should own.
     * @param tabSwitcherScrimAnchor {@link ViewGroup} used by tab switcher layout to show scrim
     *         when overview is visible.
     * @param scrimCoordinator {@link ScrimCoordinator} to show/hide scrim.
     * @return The {@link TabSwitcherAndStartSurfaceLayout}.
     */
    public static Layout createTabSwitcherAndStartSurfaceLayout(Context context,
            LayoutUpdateHost updateHost, LayoutRenderHost renderHost, StartSurface startSurface,
            JankTracker jankTracker, ViewGroup tabSwitcherScrimAnchor,
            ScrimCoordinator scrimCoordinator) {
        return new TabSwitcherAndStartSurfaceLayout(context, updateHost, renderHost, startSurface,
                jankTracker, tabSwitcherScrimAnchor, scrimCoordinator);
    }

    /**
     * Create the {@link StartSurfaceCoordinator}
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
     * @param tabCreatorManager Manages creation of tabs.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     * @param multiWindowModeStateDispatcher Gives access to the multi window mode state.
     * @param jankTracker Measures jank while tab switcher is visible.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param crowButtonDelegate The {@link CrowButtonDelegate} to handle Crow click events.
     * @param backPressManager {@link BackPressManager} to handle back press gesture.
     * @param incognitoReauthControllerSupplier {@link OneshotSupplier<IncognitoReauthController>}
     *         to detect pending re-auth when tab switcher is shown.
     * @param tabSwitcherClickHandler The {@link OnClickListener} for the tab switcher button.
     * @return the {@link StartSurface}
     */
    public static StartSurface createStartSurface(@NonNull Activity activity,
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
            @NonNull JankTracker jankTracker, @NonNull Supplier<Toolbar> toolbarSupplier,
            @NonNull CrowButtonDelegate crowButtonDelegate, BackPressManager backPressManager,
            @NonNull OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @NonNull OnClickListener tabSwitcherClickHandler) {
        return new StartSurfaceCoordinator(activity, scrimCoordinator, sheetController,
                startSurfaceOneshotSupplier, parentTabSupplier, hadWarmStart, windowAndroid,
                containerView, dynamicResourceLoaderSupplier, tabModelSelector,
                browserControlsManager, snackbarManager, shareDelegateSupplier, omniboxStubSupplier,
                tabContentManager, modalDialogManager, chromeActivityNativeDelegate,
                activityLifecycleDispatcher, tabCreatorManager, menuOrKeyboardActionController,
                multiWindowModeStateDispatcher, jankTracker, toolbarSupplier, crowButtonDelegate,
                backPressManager, incognitoReauthControllerSupplier, tabSwitcherClickHandler);
    }
}
