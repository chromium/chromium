// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;
import java.util.Map;

/** This class handles the donation of Tabs. */
public class AuxiliarySearchDonor {
    /**
     * @param context The application context.
     */
    public AuxiliarySearchDonor(@NonNull Context context) {}

    /** Creates a session and initializes the schema type. */
    void createSessionAndInit() {
        createAppSearchSession();
        setSchema();
    }

    /** Creates a session asynchronously. */
    private void createAppSearchSession() {}

    /** Sets the document schema for the current session. */
    private void setSchema() {}

    /**
     * Donates tabs with favicons.
     *
     * @param entries The list of {@link AuxiliarySearchEntry} object which contains a Tab's data.
     * @param tabIdToFaviconMap The map of <TabId, favicon>.
     */
    @VisibleForTesting
    public void donateTabs(
            @NonNull List<AuxiliarySearchEntry> entries,
            @Nullable Map<Integer, Bitmap> tabIdToFaviconMap,
            @NonNull Callback<Boolean> callback) {}

    /** Donates a list of tabs. */
    @VisibleForTesting
    public void donateTabs(@NonNull List<Tab> tabs, @NonNull Callback<Boolean> callback) {}

    /**
     * Donates tabs with favicons.
     *
     * @param tabToFaviconMap The map of tab with favicons.
     */
    @VisibleForTesting
    public void donateTabs(@NonNull Map<Tab, Bitmap> tabToFaviconMap, Callback<Boolean> callback) {}

    /** Removes all tabs for auxiliary search based on namespace. */
    @VisibleForTesting
    public void deleteAllTabs(@NonNull Callback<Boolean> onDeleteCompleteCallback) {}

    /** Closes the session. */
    @VisibleForTesting
    public void destroy() {}
}
