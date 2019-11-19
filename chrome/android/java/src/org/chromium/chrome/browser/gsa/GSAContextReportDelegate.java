// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import androidx.annotation.Nullable;

/**
 * The interface that allows the current browsing context to be reported to GSA.
 */
public interface GSAContextReportDelegate {

    /**
     * Report the current url and title (i.e. the context) to GSA. This method marks the start of a
     * context, and it should be used in pair with {@link #reportContextUsageEnded()}.
     * @param url The url for the current context.
     * @param title The page title for the current context.
     * @param displaySelection The {@code SsbContextDisplaySelection} or {@code null}.
     */
    void reportContext(String url, String title,
            @Nullable GSAContextDisplaySelection displaySelection);

    /**
     * Report the end of usage for the previously reported context.
     */
    void reportContextUsageEnded();
}
