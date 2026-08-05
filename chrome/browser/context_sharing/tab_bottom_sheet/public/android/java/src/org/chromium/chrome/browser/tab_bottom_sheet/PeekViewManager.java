// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Minimal interface for lifecycle management of the PeekView interactions. */
@NullMarked
public interface PeekViewManager {
    /** Returns the property model for the peek view. */
    PropertyModel getModel();

    /** Cleans up resources and unbinds processors. */
    void destroy();
}
