// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.common.collect.ImmutableMap;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/** Implementation of {@link PaneManager} for managing {@link Pane}s. */
public class PaneManagerImpl implements PaneManager {
    private final ObservableSupplierImpl<Pane> mCurrentPaneSupplierImpl =
            new ObservableSupplierImpl<>();
    private final ImmutableMap<Integer, LazyOneshotSupplier<Pane>> mPanes;
    private final ObservableSupplier<Boolean> mHubVisibilitySupplier;
    private final Callback<Boolean> mHubVisibilityObserver;
    private final PaneTransitionHelper mPaneTransitionHelper;
    private final PaneOrderController mPaneOrderController;

    /**
     * Create a {@link PaneManagerImpl}.
     *
     * @param paneListBuilder The {@link PaneListBuilder} consumed to build the list of {@link
     *     Pane}s to manage.
     * @param hubVisibilitySupplier The supplier for visibility of the Hub.
     */
    public PaneManagerImpl(
            PaneListBuilder paneListBuilder, ObservableSupplier<Boolean> hubVisibilitySupplier) {
        mPanes = paneListBuilder.build();
        mHubVisibilitySupplier = hubVisibilitySupplier;
        mHubVisibilityObserver = this::onHubVisibilityChanged;
        mHubVisibilitySupplier.addObserver(mHubVisibilityObserver);
        mPaneTransitionHelper = new PaneTransitionHelper(this);
        mPaneOrderController = paneListBuilder.getPaneOrderController();
    }

    /** Destroys the {@link PaneManager}. */
    public void destroy() {
        mHubVisibilitySupplier.removeObserver(mHubVisibilityObserver);
        mPaneTransitionHelper.destroy();
        for (LazyOneshotSupplier<Pane> paneSupplier : mPanes.values()) {
            if (paneSupplier == null || !paneSupplier.hasValue()) continue;

            Pane pane = paneSupplier.get();
            if (pane != null) pane.destroy();
        }
    }

    @Override
    public @NonNull PaneOrderController getPaneOrderController() {
        return mPaneOrderController;
    }

    @Override
    public @NonNull ObservableSupplier<Pane> getFocusedPaneSupplier() {
        return mCurrentPaneSupplierImpl;
    }

    @Override
    public boolean focusPane(@PaneId int paneId) {
        Pane nextPane = getPaneForId(paneId);
        if (nextPane == null || !nextPane.getReferenceButtonDataSupplier().hasValue()) {
            return false;
        }

        Pane previousPane = mCurrentPaneSupplierImpl.get();
        if (nextPane == previousPane) return true;

        RecordHistogram.recordEnumeratedHistogram("Android.Hub.PaneFocused", paneId, PaneId.COUNT);

        mCurrentPaneSupplierImpl.set(nextPane);
        if (isHubVisible()) {
            mPaneTransitionHelper.processTransition(nextPane.getPaneId(), LoadHint.HOT);
        }

        if (previousPane != null && isHubVisible()) {
            mPaneTransitionHelper.queueTransition(previousPane.getPaneId(), LoadHint.WARM);
        }
        return true;
    }

    @Override
    public @Nullable Pane getPaneForId(@PaneId int paneId) {
        LazyOneshotSupplier<Pane> paneSupplier = mPanes.get(paneId);
        if (paneSupplier == null) return null;

        Pane pane = paneSupplier.get();
        if (pane == null) return null;

        assert pane.getPaneId() == paneId
                : "Pane#getPaneId() "
                        + pane.getPaneId()
                        + " does not match the paneId it was registered with "
                        + paneId;
        return pane;
    }

    private boolean isHubVisible() {
        return Boolean.TRUE.equals(mHubVisibilitySupplier.get());
    }

    private void onHubVisibilityChanged(boolean isVisible) {
        @LoadHint int loadHint = isVisible ? LoadHint.WARM : LoadHint.COLD;

        Pane currentPane = mCurrentPaneSupplierImpl.get();
        boolean hasCurrentPane = currentPane != null;
        if (hasCurrentPane) {
            mPaneTransitionHelper.processTransition(
                    currentPane.getPaneId(), isVisible ? LoadHint.HOT : LoadHint.WARM);
        }

        for (int paneId : mPanes.keySet()) {
            if (hasCurrentPane && currentPane.getPaneId() == paneId) continue;

            mPaneTransitionHelper.queueTransition(paneId, loadHint);
        }

        // Queue this as the last transition in case the user quickly returns.
        if (hasCurrentPane && !isVisible) {
            mPaneTransitionHelper.queueTransition(currentPane.getPaneId(), LoadHint.COLD);
        }
    }
}
