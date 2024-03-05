// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/**
 * ContextualSearchSelection to be used in ContextualSearchSelectionObserver.onSelectionChanged.
 * Used LOCALLY ONLY. The context data can be quite privacy-sensitive because it contains text from
 * the page being viewed by the user, which may include sensitive or personal information. Clients
 * must follow standard privacy policy before logging or transmitting this information.
 */
public class ContextualSearchSelection {

    public final String encoding;
    public final String content;
    public final int startOffset;
    public final int endOffset;

    /**
     * Creates an immutable object representing a selection and surrounding text.
     *
     * @param encoding The encoding used for the content.
     * @param content The entire content including the surrounding text and the selection.
     * @param startOffset The offset to the start of the selection (inclusive).
     * @param endOffset The offset to the end of the selection (non-inclusive).
     */
    public ContextualSearchSelection(
            String encoding, String content, int startOffset, int endOffset) {
        this.encoding = encoding;
        this.content = content;
        this.startOffset = startOffset;
        this.endOffset = endOffset;
    }
}
