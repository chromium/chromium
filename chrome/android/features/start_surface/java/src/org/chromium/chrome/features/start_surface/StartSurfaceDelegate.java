// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

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
    public static StartSurfaceHomeLayout createStartSurfaceHomeLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            StartSurface startSurface) {
        return new StartSurfaceHomeLayout(context, updateHost, renderHost, startSurface);
    }

    /**
     * Create the {@link StartSurfaceCoordinator}
     *
     * @param activity The {@link Activity} creates this {@link StartSurface}.
     * @param sheetController A {@link BottomSheetController} to show content in the bottom sheet.
     * @param startSurfaceOneshotSupplier Supplies the {@link StartSurface}, passing the owned
     *     supplier to StartSurface itself.
     * @param parentTabSupplier A {@link Supplier} to provide parent tab for StartSurface.
     * @param hadWarmStart Whether the activity had a warm start because the native library was
     *     already fully loaded and initialized
     * @param windowAndroid An instance of a {@link WindowAndroid}
     * @param containerView The container {@link ViewGroup} for this ui, also the root view for
     *     StartSurface.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param browserControlsManager Manages the browser controls.
     * @param snackbarManager Manages the display of snackbars.
     * @param shareDelegateSupplier Supplies the current {@link ShareDelegate}.
     * @param omniboxStubSupplier Supplies the {@link OmniboxStub}.
     * @param tabContentManager Gives access to the tab content.
     * @param chromeActivityNativeDelegate Delegate for native initialization.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabCreatorManager Manages creation of tabs.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param backPressManager {@link BackPressManager} to handle back press gesture.
     * @param profileSupplier Supplies the {@link Profile}.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param moduleRegistrySupplier Supplies the {@link ModuleRegistry}.
     * @return the {@link StartSurface}
     */
    public static StartSurface createStartSurface(
            @NonNull Activity activity,
            @NonNull BottomSheetController sheetController,
            @NonNull OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            @NonNull Supplier<Tab> parentTabSupplier,
            boolean hadWarmStart,
            @NonNull WindowAndroid windowAndroid,
            @NonNull JankTracker jankTracker,
            @NonNull ViewGroup containerView,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull Supplier<OmniboxStub> omniboxStubSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull Supplier<Toolbar> toolbarSupplier,
            BackPressManager backPressManager,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ObservableSupplier<Integer> tabStripHeightSupplier,
            @Nullable OneshotSupplier<ModuleRegistry> moduleRegistrySupplier) {
        return new StartSurfaceCoordinator(
                activity,
                sheetController,
                startSurfaceOneshotSupplier,
                parentTabSupplier,
                hadWarmStart,
                windowAndroid,
                jankTracker,
                containerView,
                tabModelSelector,
                browserControlsManager,
                snackbarManager,
                shareDelegateSupplier,
                omniboxStubSupplier,
                tabContentManager,
                chromeActivityNativeDelegate,
                activityLifecycleDispatcher,
                tabCreatorManager,
                toolbarSupplier,
                backPressManager,
                profileSupplier,
                tabStripHeightSupplier,
                moduleRegistrySupplier);
    }
}
