// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.SparseArray;

/** A map associating a tab id to an {@link AsyncTabParams}. */
public interface AsyncTabParamsManager {
    /**
     * Stores AsyncTabParams used when the tab with the given ID is launched via intent.
     * @param tabId The ID of the tab that will be launched via intent.
     * @param params The AsyncTabParams to use when creating the Tab.
     */
    void add(int tabId, AsyncTabParams params);

    /**
     * @return Whether there is already an {@link AsyncTabParams} added for the given ID.
     */
    boolean hasParamsForTabId(int tabId);

    /**
     * @return Whether there are any saved {@link AsyncTabParams} with a tab to reparent. All
     *         implementations of this are keyed off of a user gesture so the likelihood of having
     *         more than one is zero.
     */
    boolean hasParamsWithTabToReparent();

    /**
     * @return A map of tab IDs to AsyncTabParams containing data that will be used later when a tab
     *         is opened via an intent.
     */
    SparseArray<AsyncTabParams> getAsyncTabParams();

    /**
     * @return Retrieves and removes AsyncTabCreationParams for a particular tab id.
     */
    AsyncTabParams remove(int tabId);
}
