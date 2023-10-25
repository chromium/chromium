// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Intent;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabLauncher;
import org.chromium.url.GURL;

import java.io.Serializable;
import java.util.List;

/**
 * Provider of functionality that the HistoryClusters component can't or shouldn't implement
 * internally.
 */
public interface HistoryClustersDelegate {
    /** Returns whether the HistoryClusters UI is running in its own activity. */
    boolean isSeparateActivity();

    /**
     * Returns the currently selected tab, if any. {@code null} when not running in a separate
     * activity.
     */
    @Nullable
    Tab getTab();

    /** Returns an intent that opens the history activity. */
    @Nullable
    Intent getHistoryActivityIntent();

    /**
     * Returns an intent that opens the given url in the correct main browsing activity, optionally
     * specifying a list of additional urls to open with the same options.
     */
    @Nullable
    <SerializableList extends List<String> & Serializable> Intent getOpenUrlIntent(GURL gurl,
            boolean inIncognito, boolean createNewTab, boolean inTabGroup,
            @Nullable SerializableList additionalUrls);

    /** Returns a toggle view that swaps between the Journeys UI and the "normal" History UI. */
    @Nullable
    ViewGroup getToggleView(ViewGroup parent);

    /** Returns an object that can launch new tabs. */
    default @Nullable AsyncTabLauncher getTabLauncher(boolean isIncognito) {
        return null;
    }

    /**
     * Returns a view containing a disclaimer about the presence of other forms of browsing
     * history.
     */
    default @Nullable ViewGroup getPrivacyDisclaimerView(ViewGroup parent) {
        return null;
    }

    /** Returns an updatable indicator of whether the privacy disclaimer should be shown. */
    default ObservableSupplier<Boolean> shouldShowPrivacyDisclaimerSupplier() {
        return new ObservableSupplierImpl<>();
    }

    /** Called when the info header's visibility should be toggled. */
    default void toggleInfoHeaderVisibility() {}

    /**
     * Returns whether the user has other forms of browsing history, indicating the need to show a
     * disclaimer.
     */
    default boolean hasOtherFormsOfBrowsingHistory() {
        return false;
    }

    /** Returns a view containing a link to a UI where the user can clear their browsing data. */
    default @Nullable ViewGroup getClearBrowsingDataView(ViewGroup parent) {
        return null;
    }

    /** Returns an updatable indicator of whether the clear browsing data link should be shown. */
    default ObservableSupplier<Boolean> shouldShowClearBrowsingDataSupplier() {
        return new ObservableSupplierImpl<>();
    }

    default void markVisitForRemoval(ClusterVisit clusterVisit) {}

    default void removeMarkedItems() {}

    /** Returns the user-facing string that should be displayed when a search has no matches. */
    default String getSearchEmptyString() {
        return "";
    }
    /**
     * Called when the user opts out of the Journeys feature to signal to the embedding component
     * that it should remove the HistoryClusters UI.
     */
    default void onOptOut() {}

    /** Whether the rename from "Journeys" to "Groups" is enabled. */
    default boolean isRenameEnabled() {
        return true;
    }
}
