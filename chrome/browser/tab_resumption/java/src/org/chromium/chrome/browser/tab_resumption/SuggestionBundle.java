// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import java.util.ArrayList;

/** Container for all data needed to render the tab resumption module. */
public class SuggestionBundle {
    public final ArrayList<SuggestionEntry> entries;
    // Reference time for recency text, in milliseconds since the epoch. Stored
    // to ensure consistency when regenerating View. Mutable to enable recency
    // text updating.
    public final long referenceTimeMs;

    public SuggestionBundle(long referenceTimeMs) {
        this.entries = new ArrayList<SuggestionEntry>();
        this.referenceTimeMs = referenceTimeMs;
    }
}
