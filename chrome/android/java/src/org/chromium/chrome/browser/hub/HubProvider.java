// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Main entrypoint for providing core Hub objects to Chrome.
 *
 * <p>Part of chrome/android/ to use {@link HubManagerFactory} and to use as glue code.
 */
public class HubProvider {
    private final LazyOneshotSupplier<HubManager> mHubManagerSupplier;
    private final PaneListBuilder mPaneListBuilder;

    /**
     * @param context The Android {@link Context} for the Hub.
     * @param orderController The {@link PaneOrderController} for the Hub.
     * @param backPressManager The {@link BackPressManager} for the activity.
     * @param tabSupplier The supplier of the current tab in the current tab model.
     */
    public HubProvider(
            @NonNull Context context,
            @NonNull PaneOrderController orderController,
            @NonNull BackPressManager backPressManager,
            @NonNull ObservableSupplier<Tab> tabSupplier) {
        mPaneListBuilder = new PaneListBuilder(orderController);
        mHubManagerSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () ->
                                HubManagerFactory.createHubManager(
                                        context, mPaneListBuilder, backPressManager, tabSupplier));
    }

    /** Returns the lazy supplier for {@link HubManager}. */
    public @NonNull LazyOneshotSupplier<HubManager> getHubManagerSupplier() {
        return mHubManagerSupplier;
    }

    /**
     * Returns the {@link PaneListBuilder} for registering Hub {@link Pane}s. Registering a pane
     * throws an {@link IllegalStateException} once {@code #get()} is invoked on the result of
     * {@link #getHubManagerSupplier()}.
     */
    public @NonNull PaneListBuilder getPaneListBuilder() {
        return mPaneListBuilder;
    }
}
