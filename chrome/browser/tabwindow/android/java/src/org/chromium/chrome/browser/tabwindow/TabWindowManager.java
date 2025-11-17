// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import android.app.Activity;
import android.util.Pair;

import org.chromium.base.Token;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collection;
import java.util.List;

/**
 * Manages multiple {@link TabModelSelector} instances, each owned by different {@link Activity}s.
 *
 * <p>Each of the 0 ~ |max-1| {@link WindowAndroid} contains 1 {@link Activity}, which contains 1
 * {@link TabModelSelector}, which contains 2 {@link TabModel}s, each of which contains n {@link
 * Tab}s.
 *
 * <p>Also manages tabs being reparented in AsyncTabParamsManager.
 *
 * <p>This is the highest level of the hierarchy of Tab containers.
 */
@NullMarked
public interface TabWindowManager {

    /**
     * An index that represents the invalid state (i.e. when the window wasn't found in the list).
     */
    @WindowId int INVALID_WINDOW_ID = -1;

    // Maximum number of TabModelSelectors since Android N that supports split screen.
    int MAX_SELECTORS_LEGACY = 3;

    // Maximum number of TabModelSelectors since Android S that supports multiple instances of
    // ChromeTabbedActivity.
    int MAX_SELECTORS_S = 5;

    // Maximum number of TabModelSelectors. Set high enough that it is functionally unlimited.
    int MAX_SELECTORS = 1000;

    String ASSERT_INDICES_MATCH_HISTOGRAM_NAME = "Android.MultiWindowMode.AssertIndicesMatch";
    String ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_NOT_REASSIGNED = ".NotReassigned";
    String ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_REASSIGNED = ".Reassigned";

    interface Observer {
        /** Called when a tab model selector is added for an activity opening. */
        default void onTabModelSelectorAdded(TabModelSelector selector) {}

        /** Called when tab state is initialized. */
        default void onTabStateInitialized() {}
    }

    /** Add an observer. */
    void addObserver(Observer observer);

    /** Removes an observer. */
    void removeObserver(Observer observer);

    /**
     * Called to request a {@link TabModelSelector} based on {@code index}. Note that the {@link
     * TabModelSelector} returned might not actually be the one related to {@code index} and {@link
     * #getIdForWindow(Activity)} should be called to grab the actual index if required.
     *
     * @param activity The activity to bind the selector to.
     * @param modalDialogManager The {@link ModalDialogManager} for the activity.
     * @param profileProviderSupplier The provider of the Profiles used in the selector.
     * @param tabCreatorManager An instance of {@link TabCreatorManager}.
     * @param nextTabPolicySupplier An instance of {@link NextTabPolicySupplier}.
     * @param multiInstanceManager An instance of {@link MultiInstanceManager}.
     * @param mismatchedIndicesHandler An instance of {@link MismatchedIndicesHandler}.
     * @param windowId The suggested id of the window that the selector should correspond to. Not
     *     guaranteed to be the index of the {@link TabModelSelector} returned.
     * @return {@link Pair} of the window id and the assigned {@link TabModelSelector}, or {@code
     *     null} if there are too many {@link TabModelSelector}s already built.
     */
    @Nullable Pair<@WindowId Integer, TabModelSelector> requestSelector(
            Activity activity,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            MultiInstanceManager multiInstanceManager,
            MismatchedIndicesHandler mismatchedIndicesHandler,
            @WindowId int windowId);

    /**
     * Creates and returns a headless selector if possible. If there's already a tabbed selector, it
     * will be returned instead, meaning you should not assume the returned selector is necessarily
     * headless. Note that the selector returned may still being initialized.
     *
     * @param windowId The window id to load the tabs for. Unlike {@link #requestSelector}, this
     *     will not be re-assigned.
     * @param profile The profile to scope everything to.
     * @return The selector to access tabs.
     */
    @Nullable TabModelSelector requestSelectorWithoutActivity(
            @WindowId int windowId, Profile profile);

    /**
     * Remove headless tracking for a given selector if any is currently available.
     *
     * @param windowId The window id that tabs might be loaded for.
     * @return Whether there was headless selectors that could be shutdown.
     */
    boolean shutdownIfHeadless(@WindowId int windowId);

    /**
     * Finds the current index of the {@link TabModelSelector} bound to {@code window}.
     *
     * @param activity The {@link Activity} to find the index of the {@link TabModelSelector} for.
     *     This uses the underlying {@link Activity} stored in the {@link WindowAndroid}.
     * @return The index of the {@link TabModelSelector} or {@link #INVALID_WINDOW_ID} if not found.
     */
    @WindowId
    int getIdForWindow(Activity activity);

    /** Returns the number of {@link TabModelSelector}s currently assigned to {@link Activity}s. */
    int getNumberOfAssignedTabModelSelectors();

    /** Returns the total number of incognito tabs across all tab model selectors. */
    int getIncognitoTabCount();

    /**
     * @param tab The tab to look for in each model.
     * @return The TabModel containing the given Tab or null if one doesn't exist.
     */
    @Nullable TabModel getTabModelForTab(Tab tab);

    /**
     * Use {@link #getTabById(int, int)} preferably and when possible for a more efficient lookup.
     *
     * @param tabId The ID of the tab in question.
     * @return Specified {@link Tab} or {@code null} if the {@link Tab} is not found.
     */
    @Nullable Tab getTabById(@TabId int tabId);

    /**
     * @param tabId The ID of the tab in question.
     * @param windowId The ID of the window that holds the tab.
     * @return Specified {@link Tab} or {@code null} if the {@link Tab} is not found.
     */
    @Nullable Tab getTabById(@TabId int tabId, @WindowId int windowId);

    /**
     * Similar to {@link #getTabById(int)} but returns a {@link TabWindowInfo}. Does not check the
     * non-window sources for the tab.
     *
     * @param tabId The id of the tab to look for.
     * @return The tab and related parents, null if it cannot be found.
     */
    @Nullable TabWindowInfo getTabWindowInfoById(@TabId int tabId);

    /**
     * @param windowId The ID of the window that holds the tab group.
     * @param tabGroupId The tab group ID of the tab group.
     * @param isIncognito Whether the grouped tabs are incognito tabs.
     * @return A list of tabs associated with the root ID, or {@code null} if no tabs are found.
     */
    @Nullable List<Tab> getGroupedTabsByWindow(
            @WindowId int windowId, Token tabGroupId, boolean isIncognito);

    /**
     * Finds the {@link TabModelSelector} bound to an Activity instance of a given id.
     *
     * @param windowId The window id of {@link TabModelSelector} to get.
     * @return Specified {@link TabModelSelector} or {@code null} if not found.
     */
    @Nullable TabModelSelector getTabModelSelectorById(@WindowId int windowId);

    /**
     * Finds the Window ID associated with a {@link TabModelSelector}. If it is not associated with
     * a window, then {@link #INVALID_WINDOW_ID} is returned.
     *
     * @param selector The {@link TabModelSelector} to check.
     */
    @WindowId
    int getWindowIdForSelector(TabModelSelector selector);

    /** Gets a Collection of all TabModelSelectors. */
    Collection<TabModelSelector> getAllTabModelSelectors();

    /** Returns whether the tab with the given id can safely be deleted. */
    boolean canTabStateBeDeleted(@TabId int tabId);

    /** Returns whether the tab with the given id can safely be deleted. */
    boolean canTabThumbnailBeDeleted(@TabId int tabId);

    /** Sets the given archived {@link TabModelSelector} singleton instance. */
    void setArchivedTabModelSelector(@Nullable TabModelSelector archivedTabModelSelector);

    /**
     * Starts to initialize tab models for all windows with data. Some may be headless.
     *
     * @param multiInstanceManager Used to fetch window ids.
     * @param profile Used to scope access.
     * @param selector The current selector for the caller, used as a fallback when window
     *     information is not available.
     */
    void keepAllTabModelsLoaded(
            MultiInstanceManager multiInstanceManager, Profile profile, TabModelSelector selector);

    /**
     * Tries to discern the correct window id that contains a tab group. This may be a like activity
     * or in a headless tab model. If the requested tab group cannot be found, then
     * INVALID_WINDOW_ID is returned.
     *
     * @param tabGroupId The group to look for.
     * @return The window id that holds the given tab group.
     */
    @WindowId
    int findWindowIdForTabGroup(Token tabGroupId);
}
