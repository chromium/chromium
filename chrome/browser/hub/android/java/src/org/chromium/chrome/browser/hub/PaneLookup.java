// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface for providing access to Panes. */
@NullMarked
public interface PaneLookup {
    @Nullable
    Pane getPaneForId(@PaneId int paneId);
}
