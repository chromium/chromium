// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.Nullable;

import com.google.common.collect.ImmutableMap;

import org.chromium.base.supplier.Supplier;

/**
 * Implementation of {@link PaneManager} for managing {@link Pane}s.
 */
public class PaneManagerImpl implements PaneManager {
    private final ImmutableMap<Integer, Supplier<Pane>> mPanes;

    private Pane mCurrentPane;

    /**
     * Create a {@link PaneManagerImpl}.
     * @param paneListBuilder The {@link PaneListBuilder} consumed to build the list of
     *                        {@link Pane}s to manage.
     */
    public PaneManagerImpl(PaneListBuilder paneListBuilder) {
        mPanes = paneListBuilder.build();
    }

    @Override
    public @Nullable Pane getFocusedPane() {
        return mCurrentPane;
    }

    @Override
    public boolean focusPane(@PaneId int paneId) {
        // TODO(crbug/1482286): This is an incomplete implementation, transitions may animate, and
        // not be completely synchronous.
        Supplier<Pane> nextPaneSupplier = mPanes.get(paneId);
        if (nextPaneSupplier == null) return false;

        Pane nextPane = nextPaneSupplier.get();
        if (nextPane == null) return false;

        mCurrentPane = nextPane;
        return true;
    }
}
