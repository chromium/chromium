// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.Nullable;

/** Interface for managing {@link Pane}s. */
public interface PaneManager {
    /**
     * Returns the currently focused {@link Pane} or null if no pane is focused.
     */
    public @Nullable Pane getFocusedPane();

    /**
     * Brings the specified {@link Pane} for {@link PaneId} into focus and returns whether focus
     * will occur. This operation may fail i.e. the Pane does not exist, is not focusable, etc.
     * @param paneId The {@link PaneId} of the {@link Pane} to attempt to focus.
     * @return whether focusing on the Pane will occur.
     */
    public boolean focusPane(@PaneId int paneId);
}
