// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes.fonts;

import android.graphics.Typeface;

/**
 * Response object for font loading requests which will either carry a Typeface
 * instance or an error code.
 */
public class TypefaceResponse {
    private static final int SUCCESS_REASON = -1;

    public final Typeface typeface;
    public final int reason;

    public TypefaceResponse(Typeface typeface) {
        this.typeface = typeface;
        this.reason = SUCCESS_REASON;
    }

    public TypefaceResponse(int reason) {
        this.typeface = null;
        this.reason = reason;
    }

    /**
     * Returns true if the response represents an error, false if it has a
     * Typeface instance.
     */
    public boolean isError() {
        return this.reason != SUCCESS_REASON || typeface == null;
    }
}
