// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;

import java.util.List;

/** Base class for data providers for the tab resumption module. */
public abstract class TabResumptionDataProvider {

    @Nullable protected Runnable mStatusChangedCallback;

    TabResumptionDataProvider() {}

    public abstract void destroy();

    /**
     * Main entry point to trigger suggestion fetch, and asynchronously passes the result to
     * `suggestionsCallback`. Suggestions can be null or empty if unavailable. If avialble, the
     * suggestions are filtered and sorted, with the most relevant one appearing first.
     *
     * @param suggestionsCallback Callback to pass suggestions, whose values can be null.
     */
    public abstract void fetchSuggestions(Callback<List<SuggestionEntry>> suggestionsCallback);

    /**
     * Sets or clears a Runnable to signal significant status change requiring UI update.
     *
     * @param statusChangedCallback The Runnable, which can be null to disable.
     */
    public void setStatusChangedCallback(@Nullable Runnable statusChangedCallback) {
        mStatusChangedCallback = statusChangedCallback;
    }

    /** Called by derived classes to signal significant status change requiring UI update. */
    protected void dispatchStatusChangedCallback() {
        if (mStatusChangedCallback != null) {
            mStatusChangedCallback.run();
        }
    }
}
