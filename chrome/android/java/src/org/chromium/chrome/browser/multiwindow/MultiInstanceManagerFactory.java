// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Creates {@link MultiInstanceManager}. */
@NullMarked
public class MultiInstanceManagerFactory {

    private static final UnownedUserDataKey<MultiInstanceManager> KEY = new UnownedUserDataKey<>();

    /**
     * Create a new {@link MultiInstanceManager}.
     *
     * @param activity The activity.
     * @param tabModelOrchestratorSupplier A supplier for the {@link TabModelOrchestrator} for the
     *     associated activity.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} for the
     *     associated activity.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the associated
     *     activity.
     * @param modalDialogManagerSupplier A supplier for the {@link ModalDialogManager}.
     * @param menuOrKeyboardActionController The {@link MenuOrKeyboardActionController} for the
     *     associated activity.
     * @param desktopWindowStateManagerSupplier A supplier for the {@link DesktopWindowStateManager}
     *     instance.
     * @return {@link MultiInstanceManager} object or {@code null} on the platform it is not needed.
     */
    public static MultiInstanceManager create(
            Activity activity,
            MonotonicObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            Supplier<DesktopWindowStateManager> desktopWindowStateManagerSupplier) {
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            return new MultiInstanceManagerApi31(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    modalDialogManagerSupplier,
                    menuOrKeyboardActionController,
                    desktopWindowStateManagerSupplier,
                    new TabReparentingDelegate(activity, tabModelOrchestratorSupplier));
        } else {
            return new MultiInstanceManagerImpl(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    menuOrKeyboardActionController);
        }
    }

    /** Return {@link MultiInstanceManager} associated with the given {@link WindowAndroid}. */
    public static @Nullable MultiInstanceManager from(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /* package */ static void attachToHost(
            UnownedUserDataHost host, MultiInstanceManager multiInstanceManager) {
        KEY.attachToHost(host, multiInstanceManager);
    }

    /* package */ static void detachFromAllHosts(MultiInstanceManager multiInstanceManager) {
        KEY.detachFromAllHosts(multiInstanceManager);
    }
}
