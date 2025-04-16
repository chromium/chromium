// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import org.chromium.build.annotations.NullMarked;

/** Allows for throttling the undo bar. */
@NullMarked
public interface UndoBarThrottle {
    /** Starts throttling the undo bar and returns a token to return when finished. */
    int startThrottling();

    /**
     * Stops throttling the undo bar, requires returning the token acquired from {@link
     * startThrottling}.
     */
    void stopThrottling(int token);
}
