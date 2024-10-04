// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.List;

/**
 * TabModelSelector is a wrapper class containing both a normal and an incognito TabModel.
 * This class helps the app know which mode it is currently in, and which TabModel it should
 * be using.
 */
public interface TabModelSelector {
    /** Should be called when the app starts showing a view with multiple tabs. */
    void onTabsViewShown();

    /**
     * Set the current model. This won't cause an animation, but will still change the stack that is
     * currently visible if the tab switcher is open.
     */
    void selectModel(boolean incognito);

    /**
     * Get a specific tab model
     * @return Never returns null.  Returns a stub when real model is uninitialized.
     */
    TabModel getModel(boolean incognito);

    /**
     * Get the {@link TabModelFilterProvider} that provides {@link TabModelFilter}.
     *
     * @return Never returns null. Returns a stub when real model is uninitialized.
     */
    TabModelFilterProvider getTabModelFilterProvider();

    /** Returns a list for the underlying models */
    List<TabModel> getModels();

    /** Returns the current tab model or a stub when real model is uninitialized. */
    @NonNull
    TabModel getCurrentModel();

    /**
     * Gets a supplier for the current tab model.
     *
     * @return A supplier for the current tab model. This may hold a null value before the {@link
     *     TabModelSelector} is initialized.
     */
    @NonNull
    ObservableSupplier<TabModel> getCurrentTabModelSupplier();

    /**
     * Convenience function to get the current tab on the current model
     * @return Current tab or null if none exists or if the model is not initialized.
     */
    @Nullable
    Tab getCurrentTab();

    /**
     * Convenience function to get the current tab id on the current model.
     * @return Id of the current tab or {@link Tab#INVALID_TAB_ID} if no tab is selected or the
     *         model is not initialized.
     */
    int getCurrentTabId();

    /** Returns a supplier for the current tab in the current model. */
    @NonNull
    ObservableSupplier<Tab> getCurrentTabSupplier();

    /**
     * Returns a supplier for the current tab count in the current model. This will update as the
     * current tab model changes so it will always contain the tab count of the current model. If
     * the tab count of a specific model is desired add an observer to that {@link TabModel}
     * directly.
     */
    @NonNull
    ObservableSupplier<Integer> getCurrentModelTabCountSupplier();

    /**
     * Convenience function to get the {@link TabModel} for a {@link Tab} specified by
     * {@code id}.
     * @param id The id of the {@link Tab} to find the {@link TabModel} for.
     * @return   The {@link TabModel} that owns the {@link Tab} specified by {@code id}.
     */
    TabModel getModelForTabId(int id);

    /**
     * TODO(crbug.com/350654700): clean up usages and remove isIncognitoSelected.
     *
     * @return If the incognito {@link TabModel} is current.
     * @deprecated Use {@link #isIncognitoBrandedModelSelected()} or {@link
     *     #isOffTheRecordModelSelected()}.
     */
    @Deprecated
    boolean isIncognitoSelected();

    /**
     * @return If the current {@link TabModel} is Incognito branded.
     * @see {@link Profile#isIncognitoBranded()}
     */
    boolean isIncognitoBrandedModelSelected();

    /**
     * @return If the current {@link TabModel} is off the record.
     * @see {@link Profile#isOffTheRecord()}
     */
    boolean isOffTheRecordModelSelected();

    /** Returns the {@link TabCreatorManager} to create tabs in this tab model selector. */
    TabCreatorManager getTabCreatorManager();

    /**
     * Opens a new tab.
     *
     * @param loadUrlParams parameters of the url load
     * @param type Describes how the new tab is being opened.
     * @param parent The parent tab for this new tab (or null if one does not exist).
     * @param incognito Whether to open the new tab in incognito mode.
     * @return The newly opened tab.
     */
    Tab openNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, boolean incognito);

    /**
     * Searches through all children models for the specified Tab and closes the tab if it exists.
     * @param tab the non-null tab to close
     * @return true if the tab was found
     */
    boolean closeTab(Tab tab);

    /** Close all tabs across all tab models */
    void closeAllTabs();

    /**
     * Close all tabs across all tab models
     * @param uponExit true iff the tabs are being closed upon application exit (after user presses
     *                 the system back button)
     */
    void closeAllTabs(boolean uponExit);

    /** Get total tab count across all tab models */
    int getTotalTabCount();

    /**
     * Search all TabModels for a tab with the specified id.
     * @return specified tab or null if tab is not found
     */
    Tab getTabById(int id);

    /**
     * Add an observer to be notified of changes to the TabModelSelector.
     * @param observer The {@link TabModelSelectorObserver} to notify.
     */
    void addObserver(TabModelSelectorObserver observer);

    /**
     * Removes an observer of TabModelSelector changes..
     * @param observer The {@link TabModelSelectorObserver} to remove.
     */
    void removeObserver(TabModelSelectorObserver observer);

    /**
     * Calls {@link TabModel#commitAllTabClosures()} on all {@link TabModel}s owned by this
     * selector.
     */
    void commitAllTabClosures();

    /**
     * @return Whether the tab state for this {@link TabModelSelector} has been initialized.
     */
    boolean isTabStateInitialized();

    /**
     * Prevents the TabModelSelector from destroying its tabs to allow for reparenting.
     *
     * This is only safe to be called immediately before destruction. After entering reparenting
     * mode, all the tabs are removed and stored in memory and on disk. The app is recreated right
     * after, so there should never be a need to "exit" reparenting mode.
     */
    void enterReparentingMode();

    /** Returns whether reparenting is in progress. */
    boolean isReparentingInProgress();

    /**
     * Subscribe an {@link IncognitoTabModelObserver} to events that the {@link IncognitoTabModel}
     * in this selector emits.  The model could be observed directly, but observing the
     * selector allows an observer to subscribe itself before the model is created.
     * @param incognitoObserver The observer to subscribe.
     */
    void addIncognitoTabModelObserver(IncognitoTabModelObserver incognitoObserver);

    /** Unsubscribe from {@link IncognitoTabModelObserver}. */
    void removeIncognitoTabModelObserver(IncognitoTabModelObserver incognitoObserver);

    /**
     * Sets the delegate to handle {@link TabModel} events that triggers an Incognito
     * re-authentication. This delegate is invoked when all the observers observing
     * onTabModelSelected event have been notified.
     *
     * @param incognitoReauthDialogDelegate A delegate which takes care of triggering an Incognito
     *         re-authentication.
     */
    void setIncognitoReauthDialogDelegate(
            IncognitoTabModelObserver.IncognitoReauthDialogDelegate incognitoReauthDialogDelegate);

    /** Destroy all owned {@link TabModel}s and {@link Tab}s referenced by this selector. */
    void destroy();
}
