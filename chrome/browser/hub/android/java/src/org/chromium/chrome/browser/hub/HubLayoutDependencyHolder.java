// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;

/**
 * Holds dependencies for initialization of {@link HubLayout}. These dependencies come from the
 * {@link ChromeTabbedActivity} level and are are available prior to {@link LayoutManagerChrome}
 * being constructed. Any dependencies already readily available from the {@link
 * LayoutManagerChrome} level are not included.
 */
public class HubLayoutDependencyHolder {
    private final @NonNull LazyOneshotSupplier<HubManager> mHubManagerSupplier;
    private final @NonNull LazyOneshotSupplier<ViewGroup> mHubRootViewGroupSupplier;
    private final @NonNull HubLayoutScrimController mScrimController;

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroupSupplier The supplier of the root view to attach the {@link Hub} to.
     * @param scrimCoordinator The browser scrim coordinator used for displaying scrims when
     *     transitioning to or from the {@link HubLayout} where applicable.
     * @param scrimAnchorViewSupplier The supplier of the anchor view to attach {@link HubLayout}
     *     scrims to. This should not return null after the HubLayout is initialized.
     * @param isIncognitoSupplier Whether the UI is currently in incognito mode. Used only for the
     *     {@link HubLayout} scrims.
     */
    public HubLayoutDependencyHolder(
            @NonNull LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull Supplier<View> scrimAnchorViewSupplier,
            @NonNull Supplier<Boolean> isIncognitoSupplier) {
        this(
                hubManagerSupplier,
                hubRootViewGroupSupplier,
                new HubLayoutScrimController(
                        scrimCoordinator, scrimAnchorViewSupplier, isIncognitoSupplier));
    }

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroup The root view to attach the {@link Hub} to.
     * @param scrimController The {@link HubLayoutScrimController} for managing scrims.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    HubLayoutDependencyHolder(
            @NonNull LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            @NonNull HubLayoutScrimController scrimController) {
        mHubManagerSupplier = hubManagerSupplier;
        mHubRootViewGroupSupplier = hubRootViewGroupSupplier;
        mScrimController = scrimController;
    }

    /** Returns the {@link HubManager} creating it if necessary. */
    public @NonNull HubManager getHubManager() {
        return mHubManagerSupplier.get();
    }

    /** Returns the root view to attach the Hub to creating it if necessary. */
    public @NonNull ViewGroup getHubRootView() {
        return mHubRootViewGroupSupplier.get();
    }

    /** Returns the {@link HubLayoutScrimController} used for the {@link HubLayout}. */
    public @NonNull HubLayoutScrimController getScrimController() {
        return mScrimController;
    }
}
