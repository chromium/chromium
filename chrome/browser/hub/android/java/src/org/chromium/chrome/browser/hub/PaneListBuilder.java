// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.common.collect.ImmutableMap;

import org.chromium.base.Log;
import org.chromium.base.supplier.LazyOneshotSupplier;

import java.util.HashMap;
import java.util.Locale;

/** Builder for creating an immutable list of all {@link Pane}s to be shown in the Hub. */
public class PaneListBuilder {
    private static final String TAG = "PaneListBuilder";

    private final PaneOrderController mPaneOrderController;

    /**
     * All {@link PaneId}s and {@link Pane}s that were registered before {@link #build()} is
     * invoked.
     */
    private @Nullable HashMap<Integer, LazyOneshotSupplier<Pane>> mRegisteredPanes =
            new HashMap<>(PaneId.COUNT);

    /**
     * Constructs a builder to assemble a list of {@link Pane}s to show in the Hub.
     * @param paneOrderController That will specify the order to show Panes in the Hub.
     */
    public PaneListBuilder(@NonNull PaneOrderController paneOrderController) {
        mPaneOrderController = paneOrderController;
    }

    /** Returns an object that holds the authoritative order of panes. */
    public PaneOrderController getPaneOrderController() {
        return mPaneOrderController;
    }

    /**
     * Register a new Pane for the Hub. This operation is invalid if {@link #build()} was invoked.
     *
     * @param paneId The {@link PaneId} to use for {@code pane}.
     * @param paneSupplier A supplier for a {@link Pane}. It is recommended that the underlying
     *     {@link Pane} is lazily initialized. {@link LazyOneshotSupplier#get()} will not be called
     *     at registration time to facilitate lazy initialization.
     */
    public PaneListBuilder registerPane(
            @PaneId int paneId, @NonNull LazyOneshotSupplier<Pane> paneSupplier) {
        if (isBuilt()) {
            throw new IllegalStateException(
                    "PaneListBuilder#build() was already invoked. Cannot add a pane for " + paneId);
        }
        assert !mRegisteredPanes.containsKey(paneId)
                : String.format(Locale.ENGLISH, "a pane for %d was already registered.", paneId);

        mRegisteredPanes.put(paneId, paneSupplier);

        return this;
    }

    /**
     * Returns whether {@link #build()} has already been invoked on this builder. If this returns
     * true, future calls to {@link #registerPane(int, Supplier)} will throw an {@link
     * IllegalStateException}.
     */
    public boolean isBuilt() {
        return mRegisteredPanes == null;
    }

    /**
     * Builds a {@link ImmutableMap} of Panes keyed by {@link PaneId} and ordered according to the
     * supplied {@link PaneOrderController}.
     */
    ImmutableMap<Integer, LazyOneshotSupplier<Pane>> build() {
        if (isBuilt()) {
            throw new IllegalStateException("PaneListBuilder#build() was already invoked");
        }

        ImmutableMap.Builder<Integer, LazyOneshotSupplier<Pane>> panesBuilder =
                ImmutableMap.builder();
        for (@PaneId int paneId : mPaneOrderController.getPaneOrder()) {
            LazyOneshotSupplier<Pane> paneSupplier = mRegisteredPanes.get(paneId);
            if (paneSupplier == null) {
                Log.d(TAG, "No Pane was registered for " + paneId);
                continue;
            }
            panesBuilder.put(paneId, paneSupplier);
            mRegisteredPanes.remove(paneId);
        }

        if (!mRegisteredPanes.isEmpty()) {
            Log.d(
                    TAG,
                    "Some registered panes were not used. PaneIds: " + mRegisteredPanes.keySet());
        }
        mRegisteredPanes = null;

        return panesBuilder.build();
    }
}
