// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.KeyEvent;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * This component handles the interactions with the autocomplete system.
 */
public interface AutocompleteCoordinator extends UrlFocusChangeListener, UrlTextChangeListener {
    /**
     * Clean up resources used by this class.
     */
    void destroy();

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider);

    /**
     * @param overviewModeBehavior A means of accessing the current OverviewModeState and a way to
     *         listen to state changes.
     */
    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior);

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    void setAutocompleteProfile(Profile profile);

    /**
     * Set the WindowAndroid instance associated with the containing Activity.
     */
    void setWindowAndroid(WindowAndroid windowAndroid);

    /**
     * @param provider A means of accessing the activity's tab.
     */
    void setActivityTabProvider(ActivityTabProvider provider);

    /**
     * @param shareDelegateSupplier A means of accessing the sharing feature.
     */
    void setShareDelegateSupplier(Supplier<ShareDelegate> shareDelegateSupplier);

    /**
     * Whether omnibox autocomplete should currently be prevented from generating suggestions.
     */
    void setShouldPreventOmniboxAutocomplete(boolean prevent);

    /**
     * @return The number of current autocomplete suggestions.
     */
    int getSuggestionCount();

    /**
     * Retrieve the omnibox suggestion at the specified index.  The index represents the ordering
     * in the underlying model.  The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    OmniboxSuggestion getSuggestionAt(int index);

    /**
     * Signals that native initialization has completed.
     */
    void onNativeInitialized();

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    void onVoiceResults(@Nullable List<VoiceRecognitionHandler.VoiceResult> results);

    /**
     * @return The current native pointer to the autocomplete results.
     */
    // TODO(tedchoc): Figure out how to remove this.
    long getCurrentNativeAutocompleteResult();

    /**
     * Update the layout direction of the suggestion list based on the parent layout direction.
     */
    void updateSuggestionListLayoutDirection();

    /**
     * Update the visuals of the autocomplete UI.
     * @param useDarkColors Whether dark colors should be applied to the UI.
     * @param isIncognito Whether the UI is for incognito mode or not.
     */
    void updateVisualsForState(boolean useDarkColors, boolean isIncognito);

    /**
     * Sets to show cached zero suggest results. This will start both caching zero suggest results
     * in shared preferences and also attempt to show them when appropriate without needing native
     * initialization.
     * @param showCachedZeroSuggestResults Whether cached zero suggest should be shown.
     */
    void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults);

    /**
     * Handle the key events associated with the suggestion list.
     *
     * @param keyCode The keycode representing what key was interacted with.
     * @param event The key event containing all meta-data associated with the event.
     * @return Whether the key event was handled.
     */
    boolean handleKeyEvent(int keyCode, KeyEvent event);

    /**
     * Trigger autocomplete for the given query.
     */
    void startAutocompleteForQuery(String query);

    /**
     * Given a search query, this will attempt to see if the query appears to be portion of a
     * properly formed URL.  If it appears to be a URL, this will return the fully qualified
     * version (i.e. including the scheme, etc...).  If the query does not appear to be a URL,
     * this will return null.
     *
     * @param query The query to be expanded into a fully qualified URL if appropriate.
     * @return The fully qualified URL or null.
     */
    String qualifyPartialURLQuery(String query);

    /**
     * Sends a zero suggest request to the server in order to pre-populate the result cache.
     */
    void prefetchZeroSuggestResults();
}
