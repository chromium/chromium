// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

/**
 * Represents a text selection and its context on a page.
 */
public class GSAContextDisplaySelection {
    public final String encoding;
    public final String content;
    public final int startOffset;
    public final int endOffset;

    /**
     * Creates an immutable object representing a selection and surrounding text.
     * @param encoding The encoding used for the content.
     * @param content The entire content including the surrounding text and the selection.
     * @param startOffset The offset to the start of the selection (inclusive).
     * @param endOffset The offset to the end of the selection (non-inclusive).
     */
    public GSAContextDisplaySelection(
            String encoding, String content, int startOffset, int endOffset) {
        this.encoding = encoding;
        this.content = content;
        this.startOffset = startOffset;
        this.endOffset = endOffset;
    }
}
