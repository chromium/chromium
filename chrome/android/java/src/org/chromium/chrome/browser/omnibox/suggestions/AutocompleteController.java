// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.VoiceSuggestionProvider;
import org.chromium.chrome.browser.omnibox.VoiceSuggestionProvider.VoiceResult;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to the native AutocompleteControllerAndroid.
 */
public class AutocompleteController {
    private static final String TAG = "cr_Autocomplete";

    // Maximum number of search/history suggestions to show.
    private static final int MAX_DEFAULT_SUGGESTION_COUNT = 5;

    // Maximum number of voice suggestions to show.
    private static final int MAX_VOICE_SUGGESTION_COUNT = 3;

    private long mNativeAutocompleteControllerAndroid;
    private long mCurrentNativeAutocompleteResult;
    private final OnSuggestionsReceivedListener mListener;
    private final VoiceSuggestionProvider mVoiceSuggestionProvider = new VoiceSuggestionProvider();

    private boolean mUseCachedZeroSuggestResults;
    private boolean mWaitingForSuggestionsToCache;

    /**
     * Listener for receiving OmniboxSuggestions.
     */
    public static interface OnSuggestionsReceivedListener {
        void onSuggestionsReceived(
                List<OmniboxSuggestion> suggestions, String inlineAutocompleteText);
    }

    public AutocompleteController(OnSuggestionsReceivedListener listener) {
        this(null, listener);
    }

    public AutocompleteController(Profile profile, OnSuggestionsReceivedListener listener) {
        if (profile != null) {
            mNativeAutocompleteControllerAndroid = nativeInit(profile);
        }
        mListener = listener;
    }

    /**
     * Resets the underlying autocomplete controller based on the specified profile.
     *
     * <p>This will implicitly stop the autocomplete suggestions, so
     * {@link #start(Profile, String, String, boolean)} must be called again to start them flowing
     * again.  This should not be an issue as changing profiles should not normally occur while
     * waiting on omnibox suggestions.
     *
     * @param profile The profile to reset the AutocompleteController with.
     */
    public void setProfile(Profile profile) {
        stop(true);
        if (profile == null) {
            mNativeAutocompleteControllerAndroid = 0;
            return;
        }

        mNativeAutocompleteControllerAndroid = nativeInit(profile);
    }

    /**
     * Use cached zero suggest results if there are any available and start caching them
     * for all zero suggest updates.
     */
    public void startCachedZeroSuggest() {
        mUseCachedZeroSuggestResults = true;
        List<OmniboxSuggestion> suggestions =
                OmniboxSuggestion.getCachedOmniboxSuggestionsForZeroSuggest();
        if (suggestions != null) mListener.onSuggestionsReceived(suggestions, "");
    }

    /**
     * Starts querying for omnibox suggestions for a given text.
     *
     * @param profile The profile to use for starting the AutocompleteController
     * @param url The URL of the current tab, used to suggest query refinements.
     * @param text The text to query autocomplete suggestions for.
     * @param cursorPosition The position of the cursor within the text.  Set to -1 if the cursor is
     *                       not focused on the text.
     * @param preventInlineAutocomplete Whether autocomplete suggestions should be prevented.
     * @param focusedFromFakebox Whether the user entered the omnibox by tapping the fakebox on the
     *                           native NTP. This should be false on all other pages.
     */
    public void start(Profile profile, String url, String text, int cursorPosition,
            boolean preventInlineAutocomplete, boolean focusedFromFakebox) {
        // crbug.com/764749
        Log.w(TAG, "starting autocomplete controller..[%b][%b]", profile == null,
                TextUtils.isEmpty(url));
        if (profile == null || TextUtils.isEmpty(url)) return;

        mNativeAutocompleteControllerAndroid = nativeInit(profile);
        // Initializing the native counterpart might still fail.
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeStart(mNativeAutocompleteControllerAndroid, text, cursorPosition, null, url,
                    preventInlineAutocomplete, false, false, true, focusedFromFakebox);
            mWaitingForSuggestionsToCache = false;
        }
    }

    /**
     * Given some string |text| that the user wants to use for navigation, determines how it should
     * be interpreted. This is a fallback in case the user didn't select a visible suggestion (e.g.
     * the user pressed enter before omnibox suggestions had been shown).
     *
     * Note: this updates the internal state of the autocomplete controller just as start() does.
     * Future calls that reference autocomplete results by index, e.g. onSuggestionSelected(),
     * should reference the returned suggestion by index 0.
     *
     * @param text The user's input text to classify (i.e. what they typed in the omnibox)
     * @param focusedFromFakebox Whether the user entered the omnibox by tapping the fakebox on the
     *                           native NTP. This should be false on all other pages.
     * @return The OmniboxSuggestion specifying where to navigate, the transition type, etc. May
     *         be null if the input is invalid.
     */
    public OmniboxSuggestion classify(String text, boolean focusedFromFakebox) {
        if (mNativeAutocompleteControllerAndroid != 0) {
            return nativeClassify(mNativeAutocompleteControllerAndroid, text, focusedFromFakebox);
        }
        return null;
    }

    /**
     * Starts a query for suggestions before any input is available from the user.
     *
     * @param profile The profile to use for starting the AutocompleteController.
     * @param omniboxText The text displayed in the omnibox.
     * @param url The url of the currently loaded web page.
     * @param title The title of the currently loaded web page.
     * @param focusedFromFakebox Whether the user entered the omnibox by tapping the fakebox on the
     *                           native NTP. This should be false on all other pages.
     */
    public void startZeroSuggest(Profile profile, String omniboxText, String url, String title,
            boolean focusedFromFakebox) {
        if (profile == null || TextUtils.isEmpty(url)) return;

        if (!NewTabPage.isNTPUrl(url)) {
            // Proactively start up a renderer, to reduce the time to display search results,
            // especially if a Service Worker is used.
            WarmupManager.getInstance().createSpareRenderProcessHost(profile);
        }
        mNativeAutocompleteControllerAndroid = nativeInit(profile);
        if (mNativeAutocompleteControllerAndroid != 0) {
            if (mUseCachedZeroSuggestResults) mWaitingForSuggestionsToCache = true;
            nativeOnOmniboxFocused(mNativeAutocompleteControllerAndroid, omniboxText, url, title,
                    focusedFromFakebox);
        }
    }

    /**
     * Stops generating autocomplete suggestions for the currently specified text from
     * {@link #start(Profile,String, String, boolean)}.
     *
     * <p>
     * Calling this method with {@code false}, will result in
     * {@link #onSuggestionsReceived(List, String, long)} being called with an empty
     * result set.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    public void stop(boolean clear) {
        if (clear) mVoiceSuggestionProvider.clearVoiceSearchResults();
        mCurrentNativeAutocompleteResult = 0;
        mWaitingForSuggestionsToCache = false;
        if (mNativeAutocompleteControllerAndroid != 0) {
            // crbug.com/764749
            Log.w(TAG, "stopping autocomplete.");
            nativeStop(mNativeAutocompleteControllerAndroid, clear);
        }
    }

    /**
     * Resets session for autocomplete controller. This happens every time we start typing
     * new input into the omnibox.
     */
    public void resetSession() {
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeResetSession(mNativeAutocompleteControllerAndroid);
        }
    }

    /**
     * Deletes an omnibox suggestion, if possible.
     * @param position The position at which the suggestion is located.
     */
    public void deleteSuggestion(int position, int hashCode) {
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeDeleteSuggestion(mNativeAutocompleteControllerAndroid, position, hashCode);
        }
    }

    /**
     * @return Native pointer to current autocomplete results.
     */
    @VisibleForTesting
    public long getCurrentNativeAutocompleteResult() {
        return mCurrentNativeAutocompleteResult;
    }

    @CalledByNative
    protected void onSuggestionsReceived(List<OmniboxSuggestion> suggestions,
            String inlineAutocompleteText, long currentNativeAutocompleteResult) {
        if (suggestions.size() > MAX_DEFAULT_SUGGESTION_COUNT) {
            // Trim to the default amount of normal suggestions we can have.
            suggestions.subList(MAX_DEFAULT_SUGGESTION_COUNT, suggestions.size()).clear();
        }

        // Run through new providers to get an updated list of suggestions.
        suggestions = mVoiceSuggestionProvider.addVoiceSuggestions(
                suggestions, MAX_VOICE_SUGGESTION_COUNT);

        mCurrentNativeAutocompleteResult = currentNativeAutocompleteResult;

        // Notify callbacks of suggestions.
        mListener.onSuggestionsReceived(suggestions, inlineAutocompleteText);
        if (mWaitingForSuggestionsToCache) {
            OmniboxSuggestion.cacheOmniboxSuggestionListForZeroSuggest(suggestions);
        }
    }

    @CalledByNative
    private void notifyNativeDestroyed() {
        mNativeAutocompleteControllerAndroid = 0;
    }

    /**
     * Called whenever a navigation happens from the omnibox to record metrics about the user's
     * interaction with the omnibox.
     *
     * @param selectedIndex The index of the suggestion that was selected.
     * @param type The type of the selected suggestion.
     * @param currentPageUrl The URL of the current page.
     * @param focusedFromFakebox Whether the user entered the omnibox by tapping the fakebox on the
     *                           native NTP. This should be false on all other pages.
     * @param elapsedTimeSinceModified The number of ms that passed between the user first
     *                                 modifying text in the omnibox and selecting a suggestion.
     * @param completedLength The length of the default match's inline autocompletion if any.
     * @param webContents The web contents for the tab where the selected suggestion will be shown.
     */
    public void onSuggestionSelected(int selectedIndex, int hashCode, int type,
            String currentPageUrl, boolean focusedFromFakebox, long elapsedTimeSinceModified,
            int completedLength, WebContents webContents) {
        assert mNativeAutocompleteControllerAndroid != 0;
        // Don't natively log voice suggestion results as we add them in Java.
        if (type == OmniboxSuggestionType.VOICE_SUGGEST) return;
        nativeOnSuggestionSelected(mNativeAutocompleteControllerAndroid, selectedIndex, hashCode,
                currentPageUrl, focusedFromFakebox, elapsedTimeSinceModified, completedLength,
                webContents);
    }

    /**
     * Pass the autocomplete controller a {@link Bundle} representing the results of a voice
     * recognition.
     * @param data A {@link Bundle} containing the results of a voice recognition.
     * @return The top voice match if one exists, {@code null} otherwise.
     */
    public VoiceResult onVoiceResults(Bundle data) {
        mVoiceSuggestionProvider.setVoiceResultsFromIntentBundle(data);
        List<VoiceResult> results = mVoiceSuggestionProvider.getResults();
        return (results != null && results.size() > 0) ? results.get(0) : null;
    }

    @CalledByNative
    private static List<OmniboxSuggestion> createOmniboxSuggestionList(int size) {
        return new ArrayList<OmniboxSuggestion>(size);
    }

    @CalledByNative
    private static void addOmniboxSuggestionToList(
            List<OmniboxSuggestion> suggestionList, OmniboxSuggestion suggestion) {
        suggestionList.add(suggestion);
    }

    @CalledByNative
    private static OmniboxSuggestion buildOmniboxSuggestion(int nativeType, boolean isSearchType,
            int relevance, int transition, String contents, int[] contentClassificationOffsets,
            int[] contentClassificationStyles, String description,
            int[] descriptionClassificationOffsets, int[] descriptionClassificationStyles,
            String answerContents, String answerType, String fillIntoEdit, String url,
            boolean isStarred, boolean isDeletable) {
        assert contentClassificationOffsets.length == contentClassificationStyles.length;
        List<MatchClassification> contentClassifications = new ArrayList<>();
        for (int i = 0; i < contentClassificationOffsets.length; i++) {
            contentClassifications.add(new MatchClassification(
                    contentClassificationOffsets[i], contentClassificationStyles[i]));
        }

        assert descriptionClassificationOffsets.length == descriptionClassificationStyles.length;
        List<MatchClassification> descriptionClassifications = new ArrayList<>();
        for (int i = 0; i < descriptionClassificationOffsets.length; i++) {
            descriptionClassifications.add(new MatchClassification(
                    descriptionClassificationOffsets[i], descriptionClassificationStyles[i]));
        }

        return new OmniboxSuggestion(nativeType, isSearchType, relevance, transition, contents,
                contentClassifications, description, descriptionClassifications, answerContents,
                answerType, fillIntoEdit, url, isStarred, isDeletable);
    }

    /**
     * Verifies whether the given OmniboxSuggestion object has the same hashCode as another
     * suggestion. This is used to validate that the native AutocompleteMatch object is in sync
     * with the Java version.
     */
    @CalledByNative
    private static boolean isEquivalentOmniboxSuggestion(
            OmniboxSuggestion suggestion, int hashCode) {
        return suggestion.hashCode() == hashCode;
    }

    /**
     * Updates aqs parameters on the selected match that we will navigate to and returns the
     * updated URL. |selected_index| is the position of the selected match and
     * |elapsed_time_since_input_change| is the time in ms between the first typed input and match
     * selection.
     *
     * @param selectedIndex The index of the autocomplete entry selected.
     * @param elapsedTimeSinceInputChange The number of ms between the time the user started
     *                                    typing in the omnibox and the time the user has selected
     *                                    a suggestion.
     * @return The url to navigate to for this match with aqs parameter updated, if we are
     *         making a Google search query.
     */
    public String updateMatchDestinationUrlWithQueryFormulationTime(
            int selectedIndex, int hashCode, long elapsedTimeSinceInputChange) {
        return nativeUpdateMatchDestinationURLWithQueryFormulationTime(
                mNativeAutocompleteControllerAndroid, selectedIndex, hashCode,
                elapsedTimeSinceInputChange);
    }

    @VisibleForTesting
    protected native long nativeInit(Profile profile);
    private native void nativeStart(long nativeAutocompleteControllerAndroid, String text,
            int cursorPosition, String desiredTld, String currentUrl,
            boolean preventInlineAutocomplete, boolean preferKeyword,
            boolean allowExactKeywordMatch, boolean wantAsynchronousMatches,
            boolean focusedFromFakebox);
    private native OmniboxSuggestion nativeClassify(
            long nativeAutocompleteControllerAndroid, String text, boolean focusedFromFakebox);
    private native void nativeStop(long nativeAutocompleteControllerAndroid, boolean clearResults);
    private native void nativeResetSession(long nativeAutocompleteControllerAndroid);
    private native void nativeOnSuggestionSelected(long nativeAutocompleteControllerAndroid,
            int selectedIndex, int hashCode, String currentPageUrl, boolean focusedFromFakebox,
            long elapsedTimeSinceModified, int completedLength, WebContents webContents);
    private native void nativeOnOmniboxFocused(long nativeAutocompleteControllerAndroid,
            String omniboxText, String currentUrl, String currentTitle, boolean focusedFromFakebox);
    private native void nativeDeleteSuggestion(
            long nativeAutocompleteControllerAndroid, int selectedIndex, int hashCode);
    private native String nativeUpdateMatchDestinationURLWithQueryFormulationTime(
            long nativeAutocompleteControllerAndroid, int selectedIndex, int hashCode,
            long elapsedTimeSinceInputChange);

    /**
     * Given a search query, this will attempt to see if the query appears to be portion of a
     * properly formed URL.  If it appears to be a URL, this will return the fully qualified
     * version (i.e. including the scheme, etc...).  If the query does not appear to be a URL,
     * this will return null.
     *
     * @param query The query to be expanded into a fully qualified URL if appropriate.
     * @return The fully qualified URL or null.
     */
    public static native String nativeQualifyPartialURLQuery(String query);

    /**
     * Sends a zero suggest request to the server in order to pre-populate the result cache.
     */
    public static native void nativePrefetchZeroSuggestResults();
}
