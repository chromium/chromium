// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface for providing access to Panes. */
@NullMarked
public interface PaneLookup {
    /**
     * Get {@link Pane} from {@link PaneId}.
     *
     * @param paneId the {@link PaneId} of the {@link Pane}.
     * @return the corresponding {@link Pane}.
     */
    @Nullable Pane getPaneForId(@PaneId int paneId);

    /**
     * Get the default {@link Pane}.
     *
     * @return the default {@link Pane}.
     */
    @Nullable Pane getDefaultPane();

    /**
     * Get the {@link PaneId} of the default {@link Pane}.
     *
     * @return the {@link PaneId} of the default {@link Pane}.
     */
    @PaneId
    int getDefaultPaneId();
}
