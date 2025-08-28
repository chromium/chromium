// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubColorMixer.OverviewModeAlphaObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;

import java.util.function.Supplier;

/**
 * Holds dependencies for initialization of {@link HubLayout}. These dependencies come from the
 * {@link ChromeTabbedActivity} level and are are available prior to {@link LayoutManagerChrome}
 * being constructed. Any dependencies already readily available from the {@link
 * LayoutManagerChrome} level are not included.
 */
@NullMarked
public class HubLayoutDependencyHolder {
    private final LazyOneshotSupplier<HubManager> mHubManagerSupplier;
    private final LazyOneshotSupplier<ViewGroup> mHubRootViewGroupSupplier;
    private final HubLayoutScrimController mScrimController;
    private final OverviewModeAlphaObserver mOnOverviewAlphaChange;
    private final @Nullable Supplier<Boolean> mXrFullSpaceModeSupplier;

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
     * @param xrFullSpaceModeSupplier Supplies current XR space mode status. True for XR full space
     *     mode, false otherwise.
     */
    public HubLayoutDependencyHolder(
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            ScrimManager scrimManager,
            Supplier<View> scrimAnchorViewSupplier,
            ObservableSupplier<Boolean> isIncognitoSupplier,
            OverviewModeAlphaObserver onOverviewAlphaChange,
            @Nullable Supplier<Boolean> xrFullSpaceModeSupplier) {
        this(
                hubManagerSupplier,
                hubRootViewGroupSupplier,
                new HubLayoutScrimController(
                        scrimManager, scrimAnchorViewSupplier, isIncognitoSupplier),
                onOverviewAlphaChange,
                xrFullSpaceModeSupplier);
    }

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroupSupplier Supplier for the root view to attach the hub to.
     * @param scrimController The {@link HubLayoutScrimController} for managing scrims.
     * @param onOverviewAlphaChange Observer to notify when alpha changes during animations.
     * @param xrFullSpaceModeSupplier Supplies current XR space mode status. True for XR full space
     *     mode, false otherwise.
     */
    @VisibleForTesting
    HubLayoutDependencyHolder(
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            HubLayoutScrimController scrimController,
            OverviewModeAlphaObserver onOverviewAlphaChange,
            @Nullable Supplier<Boolean> xrFullSpaceModeSupplier) {
        mHubManagerSupplier = hubManagerSupplier;
        mHubRootViewGroupSupplier = hubRootViewGroupSupplier;
        mScrimController = scrimController;
        mOnOverviewAlphaChange = onOverviewAlphaChange;
        mXrFullSpaceModeSupplier = xrFullSpaceModeSupplier;
    }

    /** Returns the {@link HubManager} creating it if necessary. */
    public HubManager getHubManager() {
        return assumeNonNull(mHubManagerSupplier.get());
    }

    /** Returns the root view to attach the Hub to creating it if necessary. */
    public ViewGroup getHubRootView() {
        return assumeNonNull(mHubRootViewGroupSupplier.get());
    }

    /** Returns the {@link HubLayoutScrimController} used for the {@link HubLayout}. */
    public HubLayoutScrimController getScrimController() {
        return mScrimController;
    }

    /** Returns the observer to notify when alpha changes during animations. */
    public OverviewModeAlphaObserver getOnOverviewAlphaChange() {
        return mOnOverviewAlphaChange;
    }

    /** Returns the supplier of the current status of the Full Space mode on XR. */
    public Supplier<Boolean> getXrFullSpaceModeSupplier() {
        if (mXrFullSpaceModeSupplier != null) {
            return mXrFullSpaceModeSupplier;
        }
        return () -> false;
    }
}
