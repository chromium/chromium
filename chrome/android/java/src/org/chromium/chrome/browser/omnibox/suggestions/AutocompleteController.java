// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Bridge to the native AutocompleteControllerAndroid.
 */
public class AutocompleteController {
    private static final String TAG = "Autocomplete";

    // Maximum number of voice suggestions to show.
    private static final int MAX_VOICE_SUGGESTION_COUNT = 3;

    // The delay between the Omnibox being opened and a spare renderer being started. Starting a
    // spare renderer is a very expensive operation, so this value must always be great enough for
    // the Omnibox to be fully rendered and otherwise not doing anything important but not so great
    // that the user navigates before it occurs. Experimentation between 1s, 2s, 3s found that 1s
    // was the most ideal.
    private static final int OMNIBOX_SPARE_RENDERER_DELAY_MS = 1000;

    private long mNativeAutocompleteControllerAndroid;
    private long mCurrentNativeAutocompleteResult;
    private OnSuggestionsReceivedListener mListener;
    private final VoiceSuggestionProvider mVoiceSuggestionProvider = new VoiceSuggestionProvider();

    private boolean mUseCachedZeroSuggestResults;
    private boolean mEnableNativeVoiceSuggestProvider;
    private boolean mWaitingForSuggestionsToCache;
    private Profile mProfile;

    /**
     * Listener for receiving OmniboxSuggestions.
     */
    public interface OnSuggestionsReceivedListener {
        void onSuggestionsReceived(
                AutocompleteResult autocompleteResult, String inlineAutocompleteText);
    }

    /**
     * @param listener The listener to be notified when new suggestions are available.
     */
    public void setOnSuggestionsReceivedListener(@NonNull OnSuggestionsReceivedListener listener) {
        mListener = listener;
    }

    void destroy() {
        if (mNativeAutocompleteControllerAndroid != 0) {
            AutocompleteControllerJni.get().releaseJavaObject(mNativeAutocompleteControllerAndroid);
        }
        mNativeAutocompleteControllerAndroid = 0;
    }

    /**
     * Resets the underlying autocomplete controller based on the specified profile. This function
     * returns early if there are no profile changes.
     *
     * <p>This will implicitly stop the autocomplete suggestions, so
     * {@link #start(Profile, String, String, boolean)} must be called again to start them flowing
     * again.  This should not be an issue as changing profiles should not normally occur while
     * waiting on omnibox suggestions.
     *
     * @param profile The profile to reset the AutocompleteController with.
     */
    public void setProfile(Profile profile) {
        assert mListener != null : "Ensure a listener is set prior to calling.";
        if (mProfile == profile) {
            mNativeAutocompleteControllerAndroid =
                    AutocompleteControllerJni.get().init(AutocompleteController.this, profile);
            return;
        }

        mProfile = profile;
        stop(true);
        if (profile == null) {
            mNativeAutocompleteControllerAndroid = 0;
            return;
        }

        mEnableNativeVoiceSuggestProvider = ChromeFeatureList.isEnabled(
                ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER);
        mNativeAutocompleteControllerAndroid =
                AutocompleteControllerJni.get().init(AutocompleteController.this, profile);
    }

    /**
     * Use cached zero suggest results if there are any available and start caching them
     * for all zero suggest updates.
     */
    void startCachedZeroSuggest() {
        assert mListener != null : "Ensure a listener is set prior to calling.";
        mUseCachedZeroSuggestResults = true;
        AutocompleteResult data = CachedZeroSuggestionsManager.readFromCache();
        mListener.onSuggestionsReceived(data, "");
    }

    /**
     * Starts querying for omnibox suggestions for a given text.
     *
     * @param profile The profile to use for starting the AutocompleteController
     * @param url The URL of the current tab, used to suggest query refinements.
     * @param pageClassification The page classification of the current tab.
     * @param text The text to query autocomplete suggestions for.
     * @param cursorPosition The position of the cursor within the text.  Set to -1 if the cursor is
     *                       not focused on the text.
     * @param preventInlineAutocomplete Whether autocomplete suggestions should be prevented.
     * @param queryTileId The ID of the query tile selected by the user, if any.
     * @param isQueryStartedFromTiles Whether the search query is started from query tiles.
     */
    public void start(Profile profile, String url, int pageClassification, String text,
            int cursorPosition, boolean preventInlineAutocomplete, @Nullable String queryTileId,
            boolean isQueryStartedFromTiles) {
        assert mListener != null : "Ensure a listener is set prior to calling.";
        // crbug.com/764749
        Log.w(TAG, "starting autocomplete controller..[%b][%b]", profile == null,
                TextUtils.isEmpty(url));
        if (profile == null || TextUtils.isEmpty(url)) return;

        setProfile(profile);

        // Initializing the native counterpart might still fail.
        if (mNativeAutocompleteControllerAndroid != 0) {
            AutocompleteControllerJni.get().start(mNativeAutocompleteControllerAndroid,
                    AutocompleteController.this, text, cursorPosition, null, url,
                    pageClassification, preventInlineAutocomplete, false, false, true, queryTileId,
                    isQueryStartedFromTiles);
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
     * @return The AutocompleteMatch specifying where to navigate, the transition type, etc. May
     *         be null if the input is invalid.
     */
    public AutocompleteMatch classify(String text, boolean focusedFromFakebox) {
        if (mNativeAutocompleteControllerAndroid != 0) {
            return AutocompleteControllerJni.get().classify(mNativeAutocompleteControllerAndroid,
                    AutocompleteController.this, text, focusedFromFakebox);
        }
        return null;
    }

    /**
     * Starts a query for suggestions before any input is available from the user.
     *
     * @param profile The profile to use for starting the AutocompleteController.
     * @param omniboxText The text displayed in the omnibox.
     * @param url The url of the currently loaded web page.
     * @param pageClassification The page classification of the current tab.
     * @param title The title of the currently loaded web page.
     */
    public void startZeroSuggest(
            Profile profile, String omniboxText, String url, int pageClassification, String title) {
        assert mListener != null : "Ensure a listener is set prior to calling.";
        if (profile == null || TextUtils.isEmpty(url)) return;

        // Proactively start up a renderer, to reduce the time to display search results,
        // especially if a Service Worker is used. This is done in a PostTask with a
        // experiment-configured delay so that the CPU usage associated with starting a new renderer
        // process does not impact the Omnibox initialization. Note that there's a small chance the
        // renderer will be started after the next navigation if the delay is too long, but the
        // spare renderer will probably get used anyways by a later navigation.
        if (!profile.isOffTheRecord() && !UrlUtilities.isNTPUrl(url)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SPARE_RENDERER)) {
            // It is ok for this to get called multiple times since all the requests will get
            // de-duplicated to the first one.
            PostTask.postDelayedTask(UiThreadTaskTraits.BEST_EFFORT,
                    // clang-format off
                    () -> {
                        ThreadUtils.assertOnUiThread();
                        WarmupManager.getInstance().createSpareRenderProcessHost(profile);
                    },
                    // clang-format on
                    ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                            ChromeFeatureList.OMNIBOX_SPARE_RENDERER,
                            "omnibox_spare_renderer_delay_ms", OMNIBOX_SPARE_RENDERER_DELAY_MS));
        }
        setProfile(profile);

        if (mNativeAutocompleteControllerAndroid != 0) {
            if (mUseCachedZeroSuggestResults) mWaitingForSuggestionsToCache = true;
            AutocompleteControllerJni.get().onOmniboxFocused(mNativeAutocompleteControllerAndroid,
                    AutocompleteController.this, omniboxText, url, pageClassification, title);
        }
    }

    /**
     * Stops generating autocomplete suggestions for the currently specified text from
     * {@link #start(Profile,String, String, boolean)}.
     *
     * <p>
     * Calling this method with {@code true}, will result in
     * {@link #onSuggestionsReceived(AutocompleteResult, String, long)} being called with an empty
     * result set.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    public void stop(boolean clear) {
        assert mListener != null : "Ensure a listener is set prior to calling.";
        if (clear) mVoiceSuggestionProvider.clearVoiceSearchResults();
        mCurrentNativeAutocompleteResult = 0;
        mWaitingForSuggestionsToCache = false;
        if (mNativeAutocompleteControllerAndroid != 0) {
            // crbug.com/764749
            Log.w(TAG, "stopping autocomplete.");
            AutocompleteControllerJni.get().stop(
                    mNativeAutocompleteControllerAndroid, AutocompleteController.this, clear);
        }
    }

    /**
     * Resets session for autocomplete controller. This happens every time we start typing
     * new input into the omnibox.
     */
    void resetSession() {
        if (mNativeAutocompleteControllerAndroid != 0) {
            AutocompleteControllerJni.get().resetSession(
                    mNativeAutocompleteControllerAndroid, AutocompleteController.this);
        }
    }

    /**
     * Deletes an omnibox suggestion, if possible.
     * @param position The position at which the suggestion is located.
     */
    void deleteSuggestion(int position, int hashCode) {
        if (mNativeAutocompleteControllerAndroid != 0) {
            AutocompleteControllerJni.get().deleteSuggestion(mNativeAutocompleteControllerAndroid,
                    AutocompleteController.this, position, hashCode);
        }
    }

    /**
     * @return Native pointer to current autocomplete results.
     */
    @VisibleForTesting
    long getCurrentNativeAutocompleteResult() {
        return mCurrentNativeAutocompleteResult;
    }

    @CalledByNative
    protected void onSuggestionsReceived(AutocompleteResult autocompleteResult,
            String inlineAutocompleteText, long currentNativeAutocompleteResult) {
        assert mListener != null : "Ensure a listener is set prior generating suggestions.";
        final AutocompleteResult originalResult = autocompleteResult;

        // Run through new providers to get an updated list of suggestions.
        if (!mEnableNativeVoiceSuggestProvider) {
            autocompleteResult = new AutocompleteResult(
                    mVoiceSuggestionProvider.addVoiceSuggestions(
                            autocompleteResult.getSuggestionsList(), MAX_VOICE_SUGGESTION_COUNT),
                    autocompleteResult.getGroupsDetails());
        }

        mCurrentNativeAutocompleteResult = currentNativeAutocompleteResult;

        // Notify callbacks of suggestions.
        mListener.onSuggestionsReceived(autocompleteResult, inlineAutocompleteText);

        if (mWaitingForSuggestionsToCache) {
            CachedZeroSuggestionsManager.saveToCache(originalResult);
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
     * @param disposition The window open disposition.
     * @param type The type of the selected suggestion.
     * @param currentPageUrl The URL of the current page.
     * @param pageClassification The page classification of the current tab.
     * @param elapsedTimeSinceModified The number of ms that passed between the user first
     *                                 modifying text in the omnibox and selecting a suggestion.
     * @param completedLength The length of the default match's inline autocompletion if any.
     * @param webContents The web contents for the tab where the selected suggestion will be shown.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void onSuggestionSelected(int selectedIndex, int disposition, int hashCode, int type,
            String currentPageUrl, int pageClassification, long elapsedTimeSinceModified,
            int completedLength, WebContents webContents) {
        assert mNativeAutocompleteControllerAndroid != 0;
        // Don't natively log voice suggestion results as we add them in Java.
        if (type == OmniboxSuggestionType.VOICE_SUGGEST) return;
        AutocompleteControllerJni.get().onSuggestionSelected(mNativeAutocompleteControllerAndroid,
                AutocompleteController.this, selectedIndex, disposition, hashCode, currentPageUrl,
                pageClassification, elapsedTimeSinceModified, completedLength, webContents);
    }

    /**
     * Pass the voice provider a list representing the results of a voice recognition.
     * @param results A list containing the results of a voice recognition.
     */
    void onVoiceResults(@Nullable List<VoiceResult> results) {
        if (!mEnableNativeVoiceSuggestProvider) {
            mVoiceSuggestionProvider.setVoiceResults(results);
        } else {
            if (results == null || results.size() == 0) return;
            final int count = Math.min(results.size(), MAX_VOICE_SUGGESTION_COUNT);
            String[] voiceMatches = new String[count];
            float[] confidenceScores = new float[count];
            for (int i = 0; i < count; i++) {
                voiceMatches[i] = results.get(i).getMatch();
                confidenceScores[i] = results.get(i).getConfidence();
            }
            AutocompleteControllerJni.get().setVoiceMatches(
                    mNativeAutocompleteControllerAndroid, voiceMatches, confidenceScores);
        }
    }

    /**
     * Verifies whether the given AutocompleteMatch object has the same hashCode as another
     * suggestion. This is used to validate that the native AutocompleteMatch object is in sync
     * with the Java version.
     */
    @CalledByNative
    private static boolean isEquivalentOmniboxSuggestion(
            AutocompleteMatch suggestion, int hashCode) {
        if (suggestion.hashCode() == hashCode) return true;

        // Note: these are only logged on development builds.
        // clang-format off
        Log.d(TAG, "Checked suggestion not valid: " + suggestion.getFillIntoEdit() + " ("
                   + suggestion.getType() + ")");
        // clang-format on
        return false;
    }

    /**
     * Updates aqs parameters on the selected match that we will navigate to and returns the
     * updated URL. |selectedIndex| and |hashCode| is the position and hash code of the selected
     * match. |elapsedTimeSinceInputChange| is the time in ms between the first typed input
     * and match selection.
     *
     * @param selectedIndex The index of the autocomplete entry selected.
     * @param hashCode Hash code of the AutocompleteMatch object that is selected.
     * @param elapsedTimeSinceInputChange The number of ms between the time the user started
     *                                    typing in the omnibox and the time the user has selected
     *                                    a suggestion.
     */
    GURL updateMatchDestinationUrlWithQueryFormulationTime(
            int selectedIndex, int hashCode, long elapsedTimeSinceInputChange) {
        return updateMatchDestinationUrlWithQueryFormulationTime(
                selectedIndex, hashCode, elapsedTimeSinceInputChange, null, null);
    }

    /**
     * Updates destination url on the selected match that we will navigate to and returns the
     * updated URL. |selectedIndex| and |hashCode| is the position and hash code of the selected
     * match. |elapsedTimeSinceInputChange| is the time in ms between the first typed input
     * and match selection. If |newQueryText| and |newQueryParams| is not empty, they will be
     * used to replace the existing query string and query params.
     * For example, if |elapsedTimeSinceInputChange| > 0, |newQyeryText| is "Politics news".
     * and the existing destination URL is "www.google.com/search?q=News+&aqs=chrome.0.69i...l3",
     * the returned new URL will be of the format
     * "www.google.com/search?q=Politics+news&aqs=chrome.0.69i...l3.1409j0j9" where
     * ".1409j0j9" is the encoded elapsed time.
     *
     * @param selectedIndex The index of the autocomplete entry selected.
     * @param hashCode Hash code of the AutocompleteMatch object that is selected.
     * @param elapsedTimeSinceInputChange The number of ms between the time the user started
     *                                    typing in the omnibox and the time the user has selected
     *                                    a suggestion.
     * @param newQueryText The new query string that will replace the existing one.
     * @param newQueryParams A list of search params to be appended to the query.
     * @return The url to navigate to for this match with aqs parameter, query string and parameters
     *         updated, if we are making a Google search query.
     */
    GURL updateMatchDestinationUrlWithQueryFormulationTime(int selectedIndex, int hashCode,
            long elapsedTimeSinceInputChange, String newQueryText, List<String> newQueryParams) {
        return AutocompleteControllerJni.get().updateMatchDestinationURLWithQueryFormulationTime(
                mNativeAutocompleteControllerAndroid, AutocompleteController.this, selectedIndex,
                hashCode, elapsedTimeSinceInputChange, newQueryText,
                newQueryParams == null ? null
                                       : newQueryParams.toArray(new String[newQueryParams.size()]));
    }

    /**
     * To find out if there is an open tab with the given |url|. Return the matching tab.
     *
     * @param url The URL which the tab opened with.
     * @return The tab opens |url|.
     */
    Tab findMatchingTabWithUrl(GURL url) {
        return AutocompleteControllerJni.get().findMatchingTabWithUrl(
                mNativeAutocompleteControllerAndroid, AutocompleteController.this, url);
    }

    @NativeMethods
    interface Natives {
        long init(AutocompleteController caller, Profile profile);
        void releaseJavaObject(long nativeAutocompleteControllerAndroid);
        void start(long nativeAutocompleteControllerAndroid, AutocompleteController caller,
                String text, int cursorPosition, String desiredTld, String currentUrl,
                int pageClassification, boolean preventInlineAutocomplete, boolean preferKeyword,
                boolean allowExactKeywordMatch, boolean wantAsynchronousMatches, String queryTileId,
                boolean isQueryStartedFromTiles);
        AutocompleteMatch classify(long nativeAutocompleteControllerAndroid,
                AutocompleteController caller, String text, boolean focusedFromFakebox);
        void stop(long nativeAutocompleteControllerAndroid, AutocompleteController caller,
                boolean clearResults);
        void resetSession(long nativeAutocompleteControllerAndroid, AutocompleteController caller);
        void onSuggestionSelected(long nativeAutocompleteControllerAndroid,
                AutocompleteController caller, int selectedIndex, int disposition, int hashCode,
                String currentPageUrl, int pageClassification, long elapsedTimeSinceModified,
                int completedLength, WebContents webContents);
        void onOmniboxFocused(long nativeAutocompleteControllerAndroid,
                AutocompleteController caller, String omniboxText, String currentUrl,
                int pageClassification, String currentTitle);
        void deleteSuggestion(long nativeAutocompleteControllerAndroid,
                AutocompleteController caller, int selectedIndex, int hashCode);
        GURL updateMatchDestinationURLWithQueryFormulationTime(
                long nativeAutocompleteControllerAndroid, AutocompleteController caller,
                int selectedIndex, int hashCode, long elapsedTimeSinceInputChange,
                String newQueryText, String[] newQueryParams);
        Tab findMatchingTabWithUrl(
                long nativeAutocompleteControllerAndroid, AutocompleteController caller, GURL url);
        void setVoiceMatches(long nativeAutocompleteControllerAndroid, String[] matches,
                float[] confidenceScores);
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
}
