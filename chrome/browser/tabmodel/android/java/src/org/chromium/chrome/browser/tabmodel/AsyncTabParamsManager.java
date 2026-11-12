// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.SparseArray;

import org.chromium.build.annotations.NullMarked;

/** A map associating a tab id to an {@link AsyncTabParams}. */
@NullMarked
public interface AsyncTabParamsManager {
    /**
     * Stores AsyncTabParams used when the tab with the given ID is launched via intent.
     *
     * @param tabId The ID of the tab that will be launched via intent.
     * @param params The AsyncTabParams to use when creating the Tab.
     */
    void add(int tabId, AsyncTabParams params);

    /** Returns whether there is already an {@link AsyncTabParams} added for the given ID. */
    boolean hasParamsForTabId(int tabId);

    /**
     * Returns whether there are any saved {@link AsyncTabParams} with a tab to reparent. All
     * implementations of this are keyed off of a user gesture so the likelihood of having more than
     * one is zero.
     */
    boolean hasParamsWithTabToReparent();

    /**
     * Returns whether there is an {@link AsyncTabParams} for the given ID with a tab to reparent.
     */
    boolean hasParamsWithTabToReparent(int tabId);

    /**
     * Returns a map of tab IDs to AsyncTabParams containing data that will be used later when a tab
     * is opened via an intent.
     */
    SparseArray<AsyncTabParams> getAsyncTabParams();

    /** Retrieves and removes the {@link AsyncTabParams} for the given tab ID. */
    AsyncTabParams remove(int tabId);
}
