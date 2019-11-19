// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.View;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * Provides navigation-related configuration.
 */
public interface HistoryNavigationDelegate {
    /**
     * @return {@link NavigationHandler#ActionDelegate} object.
     */
    NavigationHandler.ActionDelegate createActionDelegate();

    /**
     * @return {@link NavigationSheet#Delegate} object.
     */
    NavigationSheet.Delegate createSheetDelegate();

    /**
     * @param view {@link View} object to obtain the navigation setting from.
     * @return {@code true} if overscroll navigation is allowed to run on this page.
     */
    boolean isNavigationEnabled(View view);

    /**
     * @return {@link BottomSheetController} object.
     */
    Supplier<BottomSheetController> getBottomSheetController();

    /**
     * Observe window insets change to update navigation configutation dynamically.
     * @param view {@link View} to observe the insets change on.
     * @param runnable {@link Runnable} to execute when insets change is detected.
     *        Pass {@code null} to reset the observation.
     */
    void setWindowInsetsChangeObserver(View view, Runnable runnable);
}
