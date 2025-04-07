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
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;

import java.util.function.DoubleConsumer;

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
    private final DoubleConsumer mOnToolbarAlphaChange;

    /**
     * @param hubManagerSupplier The supplier of {@link HubManager}.
     * @param hubRootViewGroupSupplier The supplier of the root view to attach the {@link Hub} to.
     * @param scrimManager The browser scrim component used for displaying scrims when transitioning
     *     to or from the {@link HubLayout} where applicable.
     * @param scrimAnchorViewSupplier The supplier of the anchor view to attach {@link HubLayout}
     *     scrims to. This should not return null after the HubLayout is initialized.
     * @param isIncognitoSupplier Whether the UI is currently in incognito mode. Used only for the
     *     {@link HubLayout} scrims.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     */
    public HubLayoutDependencyHolder(
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            ScrimManager scrimManager,
            Supplier<View> scrimAnchorViewSupplier,
            ObservableSupplier<Boolean> isIncognitoSupplier,
            DoubleConsumer onToolbarAlphaChange) {
        this(
                hubManagerSupplier,
                hubRootViewGroupSupplier,
                new HubLayoutScrimController(
                        scrimManager, scrimAnchorViewSupplier, isIncognitoSupplier),
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
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            LazyOneshotSupplier<ViewGroup> hubRootViewGroupSupplier,
            HubLayoutScrimController scrimController,
            DoubleConsumer onToolbarAlphaChange) {
        mHubManagerSupplier = hubManagerSupplier;
        mHubRootViewGroupSupplier = hubRootViewGroupSupplier;
        mScrimController = scrimController;
        mOnToolbarAlphaChange = onToolbarAlphaChange;
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
    public DoubleConsumer getOnToolbarAlphaChange() {
        return mOnToolbarAlphaChange;
    }
}
