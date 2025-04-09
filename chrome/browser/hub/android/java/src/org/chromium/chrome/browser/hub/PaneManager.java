// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;

/** Interface for managing {@link Pane}s. */
@NullMarked
public interface PaneManager extends PaneLookup {
    /** Returns the authoritative source of the order of panes. */
    PaneOrderController getPaneOrderController();

    /** Returns an observable version of the current pane. */
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
