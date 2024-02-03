// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;

import java.util.function.DoubleConsumer;

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
    private final @NonNull DoubleConsumer mOnToolbarAlphaChange;

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroupSupplier The supplier of the root view to attach the {@link Hub} to.
     * @param scrimCoordinator The browser scrim coordinator used for displaying scrims when
     *     transitioning to or from the {@link HubLayout} where applicable.
     * @param scrimAnchorViewSupplier The supplier of the anchor view to attach {@link HubLayout}
     *     scrims to. This should not return null after the HubLayout is initialized.
     * @param isIncognitoSupplier Whether the UI is currently in incognito mode. Used only for the
     *     {@link HubLayout} scrims.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     */
    public HubLayoutDependencyHolder(
            @NonNull LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull Supplier<View> scrimAnchorViewSupplier,
            @NonNull ObservableSupplier<Boolean> isIncognitoSupplier,
            @NonNull DoubleConsumer onToolbarAlphaChange) {
        this(
                hubManagerSupplier,
                hubRootViewGroupSupplier,
                new HubLayoutScrimController(
                        scrimCoordinator, scrimAnchorViewSupplier, isIncognitoSupplier),
                onToolbarAlphaChange);
    }

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroupSupplier Supplier for the root view to attach the hub to.
     * @param scrimController The {@link HubLayoutScrimController} for managing scrims.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     */
    @VisibleForTesting
    HubLayoutDependencyHolder(
            @NonNull LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            @NonNull HubLayoutScrimController scrimController,
            @NonNull DoubleConsumer onToolbarAlphaChange) {
        mHubManagerSupplier = hubManagerSupplier;
        mHubRootViewGroupSupplier = hubRootViewGroupSupplier;
        mScrimController = scrimController;
        mOnToolbarAlphaChange = onToolbarAlphaChange;
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

    /** Returns the observer to notify when alpha changes during animations. */
    public @NonNull DoubleConsumer getOnToolbarAlphaChange() {
        return mOnToolbarAlphaChange;
    }
}
