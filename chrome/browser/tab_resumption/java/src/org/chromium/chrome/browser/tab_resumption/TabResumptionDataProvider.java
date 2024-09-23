// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;

import java.util.List;

/** Base class for data providers for the tab resumption module. */
public abstract class TabResumptionDataProvider {

    /** Factory to instantiate a TabResumptionDataProvider. */
    public interface TabResumptionDataProviderFactory {
        public TabResumptionDataProvider make();
    }

    /** Strength of fetchSuggestions() results. Inequality comparison is allowed. */
    @IntDef({ResultStrength.TENTATIVE, ResultStrength.STABLE, ResultStrength.FORCED_NULL})
    @interface ResultStrength {
        // The result is tentative: Tentative, stable, or forced-null results may follow.
        int TENTATIVE = 0;
        // The result is stable: Stable or forced-null results may follow.
        int STABLE = 1;
        // Force results to null, e.g., due to setting changes (e.g., user logging out) that make
        // data unavailable.
        int FORCED_NULL = 2;
    }

    /** Results from fetchSuggestions(). */
    public static class SuggestionsResult {
        public final @ResultStrength int strength;
        public final @Nullable List<SuggestionEntry> suggestions;

        SuggestionsResult(int strength, List<SuggestionEntry> suggestions) {
            this.strength = strength;
            this.suggestions = suggestions;
        }

        public int size() {
            return (suggestions == null) ? 0 : suggestions.size();
        }
    }

    @Nullable protected Runnable mStatusChangedCallback;

    TabResumptionDataProvider() {}

    /**
     * This may need to coordinate with fetchSuggestions(), since destroy() may get called while an
     * async call in fetchSuggestions() is in flight.
     */
    public abstract void destroy();

    /**
     * Main entry point to trigger suggestion fetch, and asynchronously passes the result to
     * `suggestionsCallback`. Suggestions can be null or empty if unavailable. If available, the
     * suggestions are filtered and sorted, with the most relevant one appearing first. This
     * function must be robust against destroy() being called while any async function calls are in
     * flight.
     *
     * @param suggestionsCallback Callback to pass suggestions, whose values can be null. The
     *     sequence of `strength` must be monotonically increasing across multiple calls, i.e., must
     *     match "(TENTATIVE)* (STABLE)* (FORCED_NULL)*".
     */
    public abstract void fetchSuggestions(Callback<SuggestionsResult> suggestionsCallback);

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
