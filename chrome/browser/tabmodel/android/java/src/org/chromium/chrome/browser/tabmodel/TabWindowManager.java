// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.util.Pair;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manages multiple {@link TabModelSelector} instances, each owned by different {@link Activity}s.
 *
 * Each of the 0 ~ |max-1| {@link WindowAndroid} contains 1 {@link Activity},
 * which contains 1 {@link TabModelSelector}, which contains 2 {@link TabModel}s,
 * each of which contains n {@link Tab}s.
 *
 * Also manages tabs being reparented in AsyncTabParamsManager.
 *
 * This is the highest level of the hierarchy of Tab containers.
 */
public interface TabWindowManager {
    // Maximum number of TabModelSelectors since Android N that supports split screen.
    public static final int MAX_SELECTORS_LEGACY = 3;

    // Maximum number of TabModelSelectors since Android S that supports multiple instances of
    // ChromeTabbedActivity.
    public static final int MAX_SELECTORS_S = 5;

    /**
     * @return The maximum number of simultaneous TabModelSelector instances in this Application.
     */
    int getMaxSimultaneousSelectors();

    /**
     * Called to request a {@link TabModelSelector} based on {@code index}. Note that the {@link
     * TabModelSelector} returned might not actually be the one related to {@code index} and {@link
     * #getIndexForWindow(Activity)} should be called to grab the actual index if required.
     *
     * @param profileProviderSupplier The provider of the Profiles used in the selector.
     * @param tabCreatorManager An instance of {@link TabCreatorManager}.
     * @param nextTabPolicySupplier An instance of {@link NextTabPolicySupplier}.
     * @param index The index of the requested {@link TabModelSelector}. Not guaranteed to be the
     *     index of the {@link TabModelSelector} returned.
     * @return {@link Pair} of the index and the {@link TabModelSelector} assigned to that index, or
     *     {@code null} if there are too many {@link TabModelSelector}s already built.
     */
    Pair<Integer, TabModelSelector> requestSelector(
            Activity activity,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            int index);

    /**
     * An index that represents the invalid state (i.e. when the window wasn't found in the list).
     */
    int INVALID_WINDOW_INDEX = -1;

    /**
     * Finds the current index of the {@link TabModelSelector} bound to {@code window}.
     * @param activity The {@link Activity} to find the index of the {@link TabModelSelector}
     *                 for.  This uses the underlying {@link Activity} stored in the
     *                 {@link WindowAndroid}.
     * @return         The index of the {@link TabModelSelector} or {@link #INVALID_WINDOW_INDEX} if
     *                 not found.
     */
    int getIndexForWindow(Activity activity);

    /**
     * @return The number of {@link TabModelSelector}s currently assigned to {@link Activity}s.
     */
    int getNumberOfAssignedTabModelSelectors();

    /**
     * @return The total number of incognito tabs across all tab model selectors.
     */
    int getIncognitoTabCount();

    /**
     * @param tab The tab to look for in each model.
     * @return The TabModel containing the given Tab or null if one doesn't exist.
     **/
    TabModel getTabModelForTab(Tab tab);

    /**
     * @param tabId The ID of the tab in question.
     * @return Specified {@link Tab} or {@code null} if the {@link Tab} is not found.
     */
    Tab getTabById(int tabId);

    /**
     * Finds the {@link TabModelSelector} bound to an Activity instance of a given index.
     * @param index The index of {@link TabModelSelector} to get.
     * @return Specified {@link TabModelSelector} or {@code null} if not found.
     */
    TabModelSelector getTabModelSelectorById(int index);
}
