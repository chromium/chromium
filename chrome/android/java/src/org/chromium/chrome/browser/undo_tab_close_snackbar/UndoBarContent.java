// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import org.chromium.build.annotations.NullMarked;

/**
 * The immutable content of one undo snackbar: the closure it describes, its text and its UMA type.
 * Used as the snackbar's action data.
 */
@NullMarked
final class UndoBarContent {
    /**
     * The closure this bar describes. Its tab ids and saved tab group sync ids are acted on by undo
     * or commit.
     */
    public final UndoClosureSnapshot snapshot;

    /** The template string used to format the snackbar message. */
    public final String template;

    /** The content string inserted into the template. */
    public final String content;

    /** The UMA type of the snackbar, a {@code Snackbar.UMA_*} value. */
    public final int umaType;

    /**
     * Constructs a content payload for the undo snackbar.
     *
     * @param snapshot The closure this bar describes.
     * @param template The template string for the snackbar message.
     * @param content The content string to be inserted into the template.
     * @param umaType The UMA type of the snackbar, a {@code Snackbar.UMA_*} value.
     */
    UndoBarContent(UndoClosureSnapshot snapshot, String template, String content, int umaType) {
        this.snapshot = snapshot;
        this.template = template;
        this.content = content;
        this.umaType = umaType;
    }
}
