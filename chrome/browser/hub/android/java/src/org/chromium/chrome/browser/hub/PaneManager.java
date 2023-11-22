// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;

/** Interface for managing {@link Pane}s. */
public interface PaneManager extends PaneLookup {
    /** Returns the authoritative source of the order of panes. */
    @NonNull
    PaneOrderController getPaneOrderController();

    /** Returns an observable version of the current pane. */
    @NonNull
    ObservableSupplier<Pane> getFocusedPaneSupplier();

    /**
     * Brings the specified {@link Pane} for {@link PaneId} into focus and returns whether focus
     * will occur. This operation may fail i.e. the Pane does not exist, is not focusable, etc.
     *
     * @param paneId The {@link PaneId} of the {@link Pane} to attempt to focus.
     * @return whether focusing on the Pane will occur. Also true if the pane was already focused.
     */
    boolean focusPane(@PaneId int paneId);
}
