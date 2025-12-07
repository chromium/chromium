// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Creates {@link MultiInstanceManager}. */
@NullMarked
public class MultiInstanceManagerFactory {

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
            ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
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
                    desktopWindowStateManagerSupplier);
        } else {
            return new MultiInstanceManagerImpl(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    menuOrKeyboardActionController);
        }
    }
}
