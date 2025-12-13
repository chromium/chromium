// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;

/**
 * Metadata to undo a tab group operation performed by {@link TabGroupModelFilter}. This interface
 * exposes some access to the underlying data to allow the undo snackbar to be displayed correctly.
 */
@NullMarked
public interface UndoGroupMetadata {
    /** Returns the tab group ID of the group that was created. */
    Token getTabGroupId();

    /** Returns whether the operation was performed in incognito mode. */
    boolean isIncognito();
}
