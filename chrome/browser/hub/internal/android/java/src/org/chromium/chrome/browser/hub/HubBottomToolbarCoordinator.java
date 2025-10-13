// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the Hub bottom toolbar. This class is responsible for setting up the component
 * that handles the bottom toolbar of the Hub.
 */
@NullMarked
public class HubBottomToolbarCoordinator {
    /** Mediator that handles business logic for the bottom toolbar. */
    private final HubBottomToolbarMediator mMediator;

    /** The delegate that provides bottom toolbar functionality. */
    private final HubBottomToolbarDelegate mDelegate;

    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    /**
     * Creates a new HubBottomToolbarCoordinator.
     *
     * @param context The current context.
     * @param container The container where the bottom toolbar should be added.
     * @param paneManager Interact with the current and all {@link Pane}s.
     * @param hubColorMixer Mixes the Hub Overview Color.
     * @param delegate The delegate that provides bottom toolbar functionality.
     */
    public HubBottomToolbarCoordinator(
            Context context,
            ViewGroup container,
            PaneManager paneManager,
            HubColorMixer hubColorMixer,
            HubBottomToolbarDelegate delegate,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        mDelegate = delegate;

        PropertyModel model =
                new PropertyModel.Builder(HubBottomToolbarProperties.ALL_BOTTOM_KEYS)
                        .with(COLOR_MIXER, hubColorMixer)
                        .build();

        // Initialize the bottom toolbar view through the delegate
        HubBottomToolbarView hubBottomToolbarView =
                mDelegate.initializeBottomToolbarView(
                        context, container, paneManager, hubColorMixer);

        if (hubBottomToolbarView != null) {
            PropertyModelChangeProcessor.create(
                    model, hubBottomToolbarView, HubBottomToolbarViewBinder::bind);
        }

        mMediator = new HubBottomToolbarMediator(model, mDelegate);

        if (hubBottomToolbarView != null) {
            mEdgeToEdgePadAdjuster =
                    EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                            hubBottomToolbarView, edgeToEdgeSupplier);
        }
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mMediator.destroy();
        mDelegate.destroy();
        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
            mEdgeToEdgePadAdjuster = null;
        }
    }
}
