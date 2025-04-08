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
import org.chromium.chrome.browser.hub.HubColorMixer.OverviewModeAlphaObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;

/**
 * Holds dependencies for initialization of {@link HubLayout}. These dependencies come from the
 * {@link ChromeTabbedActivity} level and are are available prior to {@link LayoutManagerChrome}
 * being constructed. Any dependencies already readily available from the {@link
 * LayoutManagerChrome} level are not included.
 */
public class HubLayoutDependencyHolder {
    private @NonNull final LazyOneshotSupplier<HubManager> mHubManagerSupplier;
    private @NonNull final LazyOneshotSupplier<ViewGroup> mHubRootViewGroupSupplier;
    private @NonNull final HubLayoutScrimController mScrimController;
    private @NonNull final OverviewModeAlphaObserver mOnOverviewAlphaChange;

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroupSupplier The supplier of the root view to attach the {@link Hub} to.
     * @param scrimManager The browser scrim component used for displaying scrims when transitioning
     *     to or from the {@link HubLayout} where applicable.
     * @param scrimAnchorViewSupplier The supplier of the anchor view to attach {@link HubLayout}
     *     scrims to. This should not return null after the HubLayout is initialized.
     * @param isIncognitoSupplier Whether the UI is currently in incognito mode. Used only for the
     *     {@link HubLayout} scrims.
     * @param onOverviewAlphaChange Observer to notify when overview color alpha changes during
     *     animations.
     */
    public HubLayoutDependencyHolder(
            @NonNull LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            @NonNull ScrimManager scrimManager,
            @NonNull Supplier<View> scrimAnchorViewSupplier,
            @NonNull ObservableSupplier<Boolean> isIncognitoSupplier,
            @NonNull OverviewModeAlphaObserver onOverviewAlphaChange) {
        this(
                hubManagerSupplier,
                hubRootViewGroupSupplier,
                new HubLayoutScrimController(
                        scrimManager, scrimAnchorViewSupplier, isIncognitoSupplier),
                onOverviewAlphaChange);
    }

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroupSupplier Supplier for the root view to attach the hub to.
     * @param scrimController The {@link HubLayoutScrimController} for managing scrims.
     * @param onOverviewAlphaChange Observer to notify when alpha changes during animations.
     */
    @VisibleForTesting
    HubLayoutDependencyHolder(
            @NonNull LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            @NonNull HubLayoutScrimController scrimController,
            @NonNull OverviewModeAlphaObserver onOverviewAlphaChange) {
        mHubManagerSupplier = hubManagerSupplier;
        mHubRootViewGroupSupplier = hubRootViewGroupSupplier;
        mScrimController = scrimController;
        mOnOverviewAlphaChange = onOverviewAlphaChange;
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
    public @NonNull OverviewModeAlphaObserver getOnOverviewAlphaChange() {
        return mOnOverviewAlphaChange;
    }
}
