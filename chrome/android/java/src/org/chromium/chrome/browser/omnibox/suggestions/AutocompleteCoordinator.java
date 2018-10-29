// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.view.ViewCompat;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.WindowManager;
import android.widget.ListView;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.VoiceSuggestionProvider.VoiceResult;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxResultsAdapter.OmniboxResultItem;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxResultsAdapter.OmniboxSuggestionDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsList.OmniboxSuggestionListEmbedder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarPhone;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator that handles the interactions with the autocomplete system.
 */
public class AutocompleteCoordinator
        implements OnSuggestionsReceivedListener, UrlFocusChangeListener, UrlTextChangeListener {
    private static final String TAG = "cr_Autocomplete";

    // Delay triggering the omnibox results upon key press to allow the location bar to repaint
    // with the new characters.
    private static final long OMNIBOX_SUGGESTION_START_DELAY_MS = 30;

    private final Context mContext;
    private final ViewGroup mParent;
    private final AutocompleteDelegate mDelegate;
    private final OmniboxSuggestionListEmbedder mSuggestionListEmbedder;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;

    private final OmniboxResultsAdapter mSuggestionListAdapter;
    private final AnswersImageFetcher mAnswersImageFetcher;
    private final List<OmniboxResultItem> mSuggestionItems;
    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<Runnable>();

    private ToolbarDataProvider mToolbarDataProvider;
    private OmniboxSuggestionsList mSuggestionList;
    private boolean mNativeInitialized;
    private AutocompleteController mAutocomplete;
    private boolean mSuggestionsShown;
    private boolean mSuggestionModalShown;
    private ViewGroup mOmniboxResultsContainer;
    private float mMaxRequiredWidth;
    private float mMaxMatchContentsWidth;

    // The timestamp (using SystemClock.elapsedRealtime()) at the point when the user started
    // modifying the omnibox with new input.
    private long mNewOmniboxEditSessionTimestamp = -1;
    // Set to true when the user has started typing new input in the omnibox, set to false
    // when the omnibox loses focus or becomes empty.
    private boolean mHasStartedNewOmniboxEditSession;

    /**
     * The text shown in the URL bar (user text + inline autocomplete) after the most recent set of
     * omnibox suggestions was received. When the user presses enter in the omnibox, this value is
     * compared to the URL bar text to determine whether the first suggestion is still valid.
     */
    private String mUrlTextAfterSuggestionsReceived;

    private boolean mIgnoreOmniboxItemSelection = true;

    private Runnable mShowSuggestions;
    private Runnable mRequestSuggestions;
    private DeferredOnSelectionRunnable mDeferredOnSelection;

    private boolean mShowCachedZeroSuggestResults;
    private boolean mShouldPreventOmniboxAutocomplete;

    /**
     * Provides the additional functionality to trigger and interact with autocomplete suggestions.
     */
    public interface AutocompleteDelegate {
        /**
         * Notified that the URL text has changed.
         */
        void onUrlTextChanged();

        /**
         * Notified that suggestions have changed.
         * @param autocompleteText The inline autocomplete text that can be appended to the
         *                         currently entered user text.
         */
        void onSuggestionsChanged(String autocompleteText);

        /**
         * Notified that the suggestions have been hidden.
         */
        void onSuggestionsHidden();

        /**
         * Requests the keyboard be hidden.
         */
        void hideKeyboard();

        /**
         * Requests that the given URL be loaded in the current tab.
         *
         * @param url The URL to be loaded.
         * @param transition The transition type associated with the url load.
         * @param inputStart The time the input started for the load request.
         */
        void loadUrl(String url, @PageTransition int transition, long inputStart);

        /**
         * Requests that the specified text be set as the current editing text in the omnibox.
         */
        void setOmniboxEditingText(String text);

        /**
         * @return Whether the omnibox was focused via the NTP fakebox.
         */
        boolean didFocusUrlFromFakebox();

        /**
         * @return Whether the URL currently has focus.
         */
        boolean isUrlBarFocused();

        /**
         * @return Whether a URL focus change animation is currently in progress.
         */
        boolean isUrlFocusChangeInProgress();
    }

    /**
     * Constructs a coordinator for the autocomplete system.
     *
     * @param parent The UI parent component for the autocomplete UI.
     * @param delegate The delegate to fulfill additional autocomplete requirements.
     * @param listEmbedder The embedder for controlling the display constraints of the suggestions
     *                     list.
     * @param urlBarEditingTextProvider Provider of editing text state from the UrlBar.
     */
    public AutocompleteCoordinator(ViewGroup parent, AutocompleteDelegate delegate,
            OmniboxSuggestionListEmbedder listEmbedder,
            UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
        mParent = parent;
        mContext = parent.getContext();
        mDelegate = delegate;
        mSuggestionListEmbedder = listEmbedder;
        mUrlBarEditingTextProvider = urlBarEditingTextProvider;

        mSuggestionItems = new ArrayList<OmniboxResultItem>();
        mAnswersImageFetcher = new AnswersImageFetcher();
        mSuggestionListAdapter =
                new OmniboxResultsAdapter(mContext, mSuggestionItems, mAnswersImageFetcher);
        mAutocomplete = new AutocompleteController(this);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            if (mNativeInitialized) {
                startZeroSuggest();
            } else {
                mDeferredNativeRunnables.add(() -> {
                    if (TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithAutocomplete())) {
                        startZeroSuggest();
                    }
                });
            }
            maybeShowOmniboxResultsContainer();
        } else {
            // Prevent any upcoming omnibox suggestions from showing once a URL is loaded (and as
            // a consequence the omnibox is unfocused).
            stopAutocomplete(true);

            updateOmniboxResultsContainerVisibility(false);

            mHasStartedNewOmniboxEditSession = false;
            mNewOmniboxEditSessionTimestamp = -1;
            hideSuggestions();
            mAnswersImageFetcher.clearCache();
        }
    }

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mToolbarDataProvider = toolbarDataProvider;
        mSuggestionListAdapter.setToolbarDataProvider(toolbarDataProvider);
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    public void setAutocompleteProfile(Profile profile) {
        mAutocomplete.setProfile(profile);
    }

    /**
     * Whether omnibox autocomplete should currently be prevented from generating suggestions.
     */
    public void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mShouldPreventOmniboxAutocomplete = prevent;
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mSuggestionItems.size();
    }

    /**
     * Retrieve the omnibox suggestion at the specified index.  The index represents the ordering
     * in the underlying model.  The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public OmniboxSuggestion getSuggestionAt(int index) {
        return mSuggestionItems.get(index).getSuggestion();
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mNativeInitialized = true;

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            mParent.post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();
    }

    /**
     * @return The suggestion list popup containing the omnibox results (or null if it has not yet
     *         been created).
     */
    @VisibleForTesting
    public OmniboxSuggestionsList getSuggestionList() {
        return mSuggestionList;
    }

    /**
     * @return Whether the suggestions list is currently visible.
     */
    public boolean isSuggestionsListShown() {
        return mSuggestionsShown;
    }

    /**
     * @return Whether a modal dialog triggered from the suggestions is currently visible.
     */
    public boolean isSuggestionModalShown() {
        return mSuggestionModalShown;
    }

    /**
     * @return The view containing the suggestions list.
     */
    public View getSuggestionContainerView() {
        return mOmniboxResultsContainer;
    }

    /**
     * @see AutocompleteController#onVoiceResults(Bundle)
     */
    // TODO(tedchoc): Should this coordinator own the voice query logic too?
    public VoiceResult onVoiceResults(Bundle data) {
        return mAutocomplete.onVoiceResults(data);
    }

    /**
     * @return The current native pointer to the autocomplete results.
     */
    // TODO(tedchoc): Figure out how to remove this.
    public long getCurrentNativeAutocompleteResult() {
        return mAutocomplete.getCurrentNativeAutocompleteResult();
    }

    /**
     * Initiates the mSuggestionListPopup.  Done on demand to not slow down the initial inflation of
     * the location bar.
     */
    private void initSuggestionList() {
        // Only called from onSuggestionsReceived(), which is a callback from a listener set up by
        // onNativeLibraryReady(), so this assert is safe.
        assert mNativeInitialized
                || mShowCachedZeroSuggestResults
            : "Trying to initialize native suggestions list before native init";
        if (mSuggestionList != null) return;

        // TODO(tedchoc): Investigate lazily building the suggestion list off of the UI thread.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            mSuggestionList = new OmniboxSuggestionsList(mContext, mSuggestionListEmbedder);
        }

        // Ensure the results container is initialized and add the suggestion list to it.
        initOmniboxResultsContainer();

        // Start with visibility GONE to ensure that show() is called. http://crbug.com/517438
        mSuggestionList.setVisibility(View.GONE);
        mSuggestionList.setAdapter(mSuggestionListAdapter);
        mSuggestionList.setClipToPadding(false);
        mSuggestionListAdapter.setSuggestionDelegate(new OmniboxSuggestionDelegate() {
            private long mLastActionUpTimestamp;

            @Override
            public void onSelection(OmniboxSuggestion suggestion, int position) {
                if (mShowCachedZeroSuggestResults && !mNativeInitialized) {
                    mDeferredOnSelection = new DeferredOnSelectionRunnable(suggestion, position) {
                        @Override
                        public void run() {
                            onSelection(this.mSuggestion, this.mPosition);
                        }
                    };
                    return;
                }
                String suggestionMatchUrl =
                        updateSuggestionUrlIfNeeded(suggestion, position, false);
                loadUrlFromOmniboxMatch(
                        suggestionMatchUrl, position, suggestion, mLastActionUpTimestamp);
                mDelegate.hideKeyboard();
            }

            @Override
            public void onRefineSuggestion(OmniboxSuggestion suggestion) {
                stopAutocomplete(false);
                boolean isUrlSuggestion = suggestion.isUrlSuggestion();
                String refineText = suggestion.getFillIntoEdit();
                if (!isUrlSuggestion) refineText = TextUtils.concat(refineText, " ").toString();

                mDelegate.setOmniboxEditingText(refineText);
                onTextChangedForAutocomplete();
                if (isUrlSuggestion) {
                    RecordUserAction.record("MobileOmniboxRefineSuggestion.Url");
                } else {
                    RecordUserAction.record("MobileOmniboxRefineSuggestion.Search");
                }
            }

            @Override
            public void onLongPress(OmniboxSuggestion suggestion, int position) {
                RecordUserAction.record("MobileOmniboxDeleteGesture");
                if (!suggestion.isDeletable()) return;

                // TODO(tedchoc): Migrate to modal dialog manager.
                AlertDialog.Builder b =
                        new AlertDialog.Builder(mParent.getContext(), R.style.AlertDialogTheme);
                b.setTitle(suggestion.getDisplayText());
                b.setMessage(R.string.omnibox_confirm_delete);

                DialogInterface.OnClickListener clickListener =
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                if (which == DialogInterface.BUTTON_POSITIVE) {
                                    RecordUserAction.record("MobileOmniboxDeleteRequested");
                                    mAutocomplete.deleteSuggestion(position, suggestion.hashCode());
                                } else if (which == DialogInterface.BUTTON_NEGATIVE) {
                                    dialog.cancel();
                                }
                            }
                        };
                b.setPositiveButton(android.R.string.ok, clickListener);
                b.setNegativeButton(android.R.string.cancel, clickListener);

                AlertDialog dialog = b.create();
                dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(DialogInterface dialog) {
                        mSuggestionModalShown = false;
                    }
                });

                mSuggestionModalShown = true;
                try {
                    dialog.show();
                } catch (WindowManager.BadTokenException ex) {
                    mSuggestionModalShown = false;
                }
            }

            @Override
            public void onSetUrlToSuggestion(OmniboxSuggestion suggestion) {
                if (mIgnoreOmniboxItemSelection) return;
                mDelegate.setOmniboxEditingText(suggestion.getFillIntoEdit());
                mIgnoreOmniboxItemSelection = true;
            }

            @Override
            public void onGestureDown() {
                stopAutocomplete(false);
            }

            @Override
            public void onGestureUp(long timestamp) {
                mLastActionUpTimestamp = timestamp;
            }

            @Override
            public void onTextWidthsUpdated(float requiredWidth, float matchContentsWidth) {
                mMaxRequiredWidth = Math.max(mMaxRequiredWidth, requiredWidth);
                mMaxMatchContentsWidth = Math.max(mMaxMatchContentsWidth, matchContentsWidth);
            }

            @Override
            public float getMaxRequiredWidth() {
                return mMaxRequiredWidth;
            }

            @Override
            public float getMaxMatchContentsWidth() {
                return mMaxMatchContentsWidth;
            }
        });
    }

    /**
     * Updates the maximum widths required to render the suggestions.
     * This is needed for infinite suggestions where we try to vertically align the leading
     * ellipsis.
     */
    private void resetMaxTextWidths() {
        mMaxRequiredWidth = 0;
        mMaxMatchContentsWidth = 0;
    }

    /**
     * Handles showing/hiding the suggestions list.
     * @param visible Whether the suggestions list should be visible.
     */
    private void setSuggestionsListVisibility(final boolean visible) {
        if (mSuggestionsShown == visible) return;
        mSuggestionsShown = visible;
        if (mSuggestionList != null) {
            final boolean isShowing = mSuggestionList.getVisibility() == View.VISIBLE;
            if (visible && !isShowing) {
                mIgnoreOmniboxItemSelection = true; // Reset to default value.

                if (mSuggestionList.getParent() == null) {
                    mOmniboxResultsContainer.addView(mSuggestionList);
                }

                mSuggestionList.show();
                updateSuggestionListLayoutDirection();
            } else if (!visible && isShowing) {
                mSuggestionList.setVisibility(View.GONE);

                UiUtils.removeViewFromParent(mSuggestionList);
            }
        }
        maybeShowOmniboxResultsContainer();
    }

    private void initOmniboxResultsContainer() {
        if (mOmniboxResultsContainer != null) return;
        ViewStub overlayStub =
                (ViewStub) mParent.getRootView().findViewById(R.id.omnibox_results_container_stub);
        mOmniboxResultsContainer = (ViewGroup) overlayStub.inflate();
    }

    /**
     * Conditionally show the omnibox suggestions container.
     */
    private void maybeShowOmniboxResultsContainer() {
        if (isSuggestionsListShown() || mDelegate.isUrlBarFocused()) {
            initOmniboxResultsContainer();
            updateOmniboxResultsContainerVisibility(true);
        }
    }

    /**
     * Update whether the omnibox suggestions container is visible.
     */
    private void updateOmniboxResultsContainerVisibility(boolean visible) {
        if (mOmniboxResultsContainer == null) return;

        boolean currentlyVisible = mOmniboxResultsContainer.getVisibility() == View.VISIBLE;
        if (currentlyVisible == visible) return;

        if (visible) {
            mOmniboxResultsContainer.setVisibility(View.VISIBLE);
        } else {
            mOmniboxResultsContainer.setVisibility(View.INVISIBLE);
        }
    }

    /**
     * Update the layout direction of the suggestion list based on the parent layout direction.
     */
    public void updateSuggestionListLayoutDirection() {
        if (mSuggestionList == null) return;
        int layoutDirection = ViewCompat.getLayoutDirection(mParent);
        mSuggestionList.updateSuggestionsLayoutDirection(layoutDirection);
        mSuggestionListAdapter.setLayoutDirection(layoutDirection);
    }

    /**
     * Handle the key events associated with the suggestion list.
     *
     * @param keyCode The keycode representing what key was interacted with.
     * @param event The key event containing all meta-data associated with the event.
     * @return Whether the key event was handled.
     */
    public boolean handleKeyEvent(int keyCode, KeyEvent event) {
        if (KeyNavigationUtil.isGoDown(event) && mSuggestionList != null
                && mSuggestionList.isShown()) {
            int suggestionCount = mSuggestionListAdapter.getCount();
            if (mSuggestionList.getSelectedItemPosition() < suggestionCount - 1) {
                if (suggestionCount > 0) mIgnoreOmniboxItemSelection = false;
            } else {
                // Do not pass down events when the last item is already selected as it will
                // dismiss the suggestion list.
                return true;
            }

            if (mSuggestionList.getSelectedItemPosition() == ListView.INVALID_POSITION) {
                // When clearing the selection after a text change, state is not reset
                // correctly so hitting down again will cause it to start from the previous
                // selection point. We still have to send the key down event to let the list
                // view items take focus, but then we select the first item explicitly.
                boolean result = mSuggestionList.onKeyDown(keyCode, event);
                mSuggestionList.setSelection(0);
                return result;
            } else {
                return mSuggestionList.onKeyDown(keyCode, event);
            }
        } else if (KeyNavigationUtil.isGoUp(event) && mSuggestionList != null
                && mSuggestionList.isShown()) {
            if (mSuggestionList.getSelectedItemPosition() != 0
                    && mSuggestionListAdapter.getCount() > 0) {
                mIgnoreOmniboxItemSelection = false;
            }
            return mSuggestionList.onKeyDown(keyCode, event);
        } else if (KeyNavigationUtil.isGoRight(event) && mSuggestionList != null
                && mSuggestionList.isShown()
                && mSuggestionList.getSelectedItemPosition() != ListView.INVALID_POSITION) {
            OmniboxResultItem selectedItem = (OmniboxResultItem) mSuggestionListAdapter.getItem(
                    mSuggestionList.getSelectedItemPosition());
            mDelegate.setOmniboxEditingText(selectedItem.getSuggestion().getFillIntoEdit());
            onTextChangedForAutocomplete();
            mSuggestionList.setSelection(0);
            return true;
        } else if (KeyNavigationUtil.isEnter(event) && mParent.getVisibility() == View.VISIBLE) {
            mDelegate.hideKeyboard();
            final String urlText = mUrlBarEditingTextProvider.getTextWithAutocomplete();
            if (mNativeInitialized) {
                findMatchAndLoadUrl(urlText, event.getEventTime());
            } else {
                mDeferredNativeRunnables.add(
                        () -> findMatchAndLoadUrl(urlText, event.getEventTime()));
            }
            return true;
        }
        return false;
    }

    private void findMatchAndLoadUrl(String urlText, long inputStart) {
        int suggestionMatchPosition;
        OmniboxSuggestion suggestionMatch;
        boolean skipOutOfBoundsCheck = false;

        if (mSuggestionList != null && mSuggestionList.isShown()
                && mSuggestionList.getSelectedItemPosition() != ListView.INVALID_POSITION) {
            // Bluetooth keyboard case: the user highlighted a suggestion with the arrow
            // keys, then pressed enter.
            suggestionMatchPosition = mSuggestionList.getSelectedItemPosition();
            OmniboxResultItem selectedItem =
                    (OmniboxResultItem) mSuggestionListAdapter.getItem(suggestionMatchPosition);
            suggestionMatch = selectedItem.getSuggestion();
        } else if (!mSuggestionItems.isEmpty()
                && urlText.equals(mUrlTextAfterSuggestionsReceived)) {
            // Common case: the user typed something, received suggestions, then pressed enter.
            suggestionMatch = mSuggestionItems.get(0).getSuggestion();
            suggestionMatchPosition = 0;
        } else {
            // Less common case: there are no valid omnibox suggestions. This can happen if the
            // user tapped the URL bar to dismiss the suggestions, then pressed enter. This can
            // also happen if the user presses enter before any suggestions have been received
            // from the autocomplete controller.
            suggestionMatch = mAutocomplete.classify(urlText, mDelegate.didFocusUrlFromFakebox());
            suggestionMatchPosition = 0;
            // Classify matches don't propagate to java, so skip the OOB check.
            skipOutOfBoundsCheck = true;

            // If urlText couldn't be classified, bail.
            if (suggestionMatch == null) return;
        }

        String suggestionMatchUrl = updateSuggestionUrlIfNeeded(
                suggestionMatch, suggestionMatchPosition, skipOutOfBoundsCheck);
        loadUrlFromOmniboxMatch(
                suggestionMatchUrl, suggestionMatchPosition, suggestionMatch, inputStart);
    }

    /**
     * Updates the URL we will navigate to from suggestion, if needed. This will update the search
     * URL to be of the corpus type if query in the omnibox is displayed and update aqs= parameter
     * on regular web search URLs.
     *
     * @param suggestion The chosen omnibox suggestion.
     * @param selectedIndex The index of the chosen omnibox suggestion.
     * @param skipCheck Whether to skip an out of bounds check.
     * @return The url to navigate to.
     */
    @SuppressWarnings("ReferenceEquality")
    private String updateSuggestionUrlIfNeeded(
            OmniboxSuggestion suggestion, int selectedIndex, boolean skipCheck) {
        // Only called once we have suggestions, and don't have a listener though which we can
        // receive suggestions until the native side is ready, so this is safe
        assert mNativeInitialized
            : "updateSuggestionUrlIfNeeded called before native initialization";

        String updatedUrl = null;
        if (suggestion.getType() != OmniboxSuggestionType.VOICE_SUGGEST) {
            int verifiedIndex = -1;
            if (!skipCheck) {
                if (mSuggestionItems.size() > selectedIndex
                        && mSuggestionItems.get(selectedIndex).getSuggestion() == suggestion) {
                    verifiedIndex = selectedIndex;
                } else {
                    // Underlying omnibox results may have changed since the selection was made,
                    // find the suggestion item, if possible.
                    for (int i = 0; i < mSuggestionItems.size(); i++) {
                        if (suggestion.equals(mSuggestionItems.get(i).getSuggestion())) {
                            verifiedIndex = i;
                            break;
                        }
                    }
                }
            }

            // If we do not have the suggestion as part of our results, skip the URL update.
            if (verifiedIndex == -1) return suggestion.getUrl();

            // TODO(mariakhomenko): Ideally we want to update match destination URL with new aqs
            // for query in the omnibox and voice suggestions, but it's currently difficult to do.
            long elapsedTimeSinceInputChange = mNewOmniboxEditSessionTimestamp > 0
                    ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                    : -1;
            updatedUrl = mAutocomplete.updateMatchDestinationUrlWithQueryFormulationTime(
                    verifiedIndex, suggestion.hashCode(), elapsedTimeSinceInputChange);
        }

        return updatedUrl == null ? suggestion.getUrl() : updatedUrl;
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     */
    @Override
    public void onTextChangedForAutocomplete() {
        // crbug.com/764749
        Log.w(TAG, "onTextChangedForAutocomplete");

        if (mShouldPreventOmniboxAutocomplete) return;

        cancelPendingAutocompleteStart();

        if (!mHasStartedNewOmniboxEditSession && mNativeInitialized) {
            mAutocomplete.resetSession();
            mNewOmniboxEditSessionTimestamp = SystemClock.elapsedRealtime();
            mHasStartedNewOmniboxEditSession = true;
        }

        if (!mParent.isInTouchMode() && mSuggestionList != null) {
            mSuggestionList.setSelection(0);
        }

        stopAutocomplete(false);
        if (TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithoutAutocomplete())) {
            // crbug.com/764749
            Log.w(TAG, "onTextChangedForAutocomplete: url is empty");
            hideSuggestions();
            startZeroSuggest();
        } else {
            assert mRequestSuggestions == null : "Multiple omnibox requests in flight.";
            mRequestSuggestions = () -> {
                String textWithoutAutocomplete =
                        mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
                boolean preventAutocomplete = !mUrlBarEditingTextProvider.shouldAutocomplete();
                mRequestSuggestions = null;

                if (!mToolbarDataProvider.hasTab()) {
                    // crbug.com/764749
                    Log.w(TAG, "onTextChangedForAutocomplete: no tab");
                    return;
                }

                Profile profile = mToolbarDataProvider.getProfile();
                int cursorPosition = -1;
                if (mUrlBarEditingTextProvider.getSelectionStart()
                        == mUrlBarEditingTextProvider.getSelectionEnd()) {
                    // Conveniently, if there is no selection, those two functions return -1,
                    // exactly the same value needed to pass to start() to indicate no cursor
                    // position.  Hence, there's no need to check for -1 here explicitly.
                    cursorPosition = mUrlBarEditingTextProvider.getSelectionStart();
                }
                mAutocomplete.start(profile, mToolbarDataProvider.getCurrentUrl(),
                        textWithoutAutocomplete, cursorPosition, preventAutocomplete,
                        mDelegate.didFocusUrlFromFakebox());
            };
            if (mNativeInitialized) {
                mParent.postDelayed(mRequestSuggestions, OMNIBOX_SUGGESTION_START_DELAY_MS);
            } else {
                mDeferredNativeRunnables.add(mRequestSuggestions);
            }
        }

        mDelegate.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsReceived(
            List<OmniboxSuggestion> newSuggestions, String inlineAutocompleteText) {
        if (mShouldPreventOmniboxAutocomplete) return;

        // This is a callback from a listener that is set up by onNativeLibraryReady,
        // so can only be called once the native side is set up unless we are showing
        // cached java-only suggestions.
        assert mNativeInitialized
                || mShowCachedZeroSuggestResults
            : "Native suggestions received before native side intialialized";

        if (mDeferredOnSelection != null) {
            mDeferredOnSelection.setShouldLog(newSuggestions.size() > mDeferredOnSelection.mPosition
                    && mDeferredOnSelection.mSuggestion.equals(
                               newSuggestions.get(mDeferredOnSelection.mPosition)));
            mDeferredOnSelection.run();
            mDeferredOnSelection = null;
        }
        String userText = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        mUrlTextAfterSuggestionsReceived = userText + inlineAutocompleteText;

        boolean itemsChanged = false;
        // If the length of the incoming suggestions matches that of those currently being shown,
        // replace them inline to allow transient entries to retain their proper highlighting.
        if (mSuggestionItems.size() == newSuggestions.size()) {
            for (int index = 0; index < newSuggestions.size(); index++) {
                OmniboxResultItem suggestionItem = mSuggestionItems.get(index);
                OmniboxSuggestion suggestion = suggestionItem.getSuggestion();
                OmniboxSuggestion newSuggestion = newSuggestions.get(index);
                // Determine whether the suggestions have changed. If not, save some time by not
                // redrawing the suggestions UI.
                if (suggestion.equals(newSuggestion)
                        && suggestion.getType() != OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) {
                    if (suggestionItem.getMatchedQuery().equals(userText)) {
                        continue;
                    } else if (!suggestion.getDisplayText().startsWith(userText)
                            && !suggestion.getUrl().contains(userText)) {
                        continue;
                    }
                }
                mSuggestionItems.set(index, new OmniboxResultItem(newSuggestion, userText));
                itemsChanged = true;
            }
        } else {
            itemsChanged = true;
            clearSuggestions(false);
            for (int i = 0; i < newSuggestions.size(); i++) {
                mSuggestionItems.add(new OmniboxResultItem(newSuggestions.get(i), userText));
            }
        }

        if (mSuggestionItems.isEmpty()) {
            if (mSuggestionsShown) {
                hideSuggestions();
            } else {
                mSuggestionListAdapter.notifySuggestionsChanged();
            }
            return;
        }

        mDelegate.onSuggestionsChanged(inlineAutocompleteText);

        // Show the suggestion list.
        initSuggestionList(); // It may not have been initialized yet.
        resetMaxTextWidths();

        if (itemsChanged) mSuggestionListAdapter.notifySuggestionsChanged();

        if (mDelegate.isUrlBarFocused()) {
            if (mShowSuggestions != null) mParent.removeCallbacks(mShowSuggestions);
            mShowSuggestions = () -> {
                setSuggestionsListVisibility(true);
                mShowSuggestions = null;
            };
            if (!mDelegate.isUrlFocusChangeInProgress()) {
                mShowSuggestions.run();
            } else {
                mParent.postDelayed(
                        mShowSuggestions, ToolbarPhone.URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            }
        }
    }

    private void loadUrlFromOmniboxMatch(
            String url, int matchPosition, OmniboxSuggestion suggestion, long inputStart) {
        // loadUrl modifies AutocompleteController's state clearing the native
        // AutocompleteResults needed by onSuggestionsSelected. Therefore,
        // loadUrl should should be invoked last.
        int transition = suggestion.getTransition();
        int type = suggestion.getType();
        String currentPageUrl = mToolbarDataProvider.getCurrentUrl();
        WebContents webContents = mToolbarDataProvider.hasTab()
                ? mToolbarDataProvider.getTab().getWebContents()
                : null;
        long elapsedTimeSinceModified = mNewOmniboxEditSessionTimestamp > 0
                ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                : -1;
        boolean shouldSkipNativeLog = mShowCachedZeroSuggestResults
                && (mDeferredOnSelection != null) && !mDeferredOnSelection.shouldLog();
        if (!shouldSkipNativeLog) {
            int autocompleteLength = mUrlBarEditingTextProvider.getTextWithAutocomplete().length()
                    - mUrlBarEditingTextProvider.getTextWithoutAutocomplete().length();
            mAutocomplete.onSuggestionSelected(matchPosition, suggestion.hashCode(), type,
                    currentPageUrl, mDelegate.didFocusUrlFromFakebox(), elapsedTimeSinceModified,
                    autocompleteLength, webContents);
        }
        if (((transition & PageTransition.CORE_MASK) == PageTransition.TYPED)
                && TextUtils.equals(url, mToolbarDataProvider.getCurrentUrl())) {
            // When the user hit enter on the existing permanent URL, treat it like a
            // reload for scoring purposes.  We could detect this by just checking
            // user_input_in_progress_, but it seems better to treat "edits" that end
            // up leaving the URL unchanged (e.g. deleting the last character and then
            // retyping it) as reloads too.  We exclude non-TYPED transitions because if
            // the transition is GENERATED, the user input something that looked
            // different from the current URL, even if it wound up at the same place
            // (e.g. manually retyping the same search query), and it seems wrong to
            // treat this as a reload.
            transition = PageTransition.RELOAD;
        } else if (type == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                && mUrlBarEditingTextProvider.wasLastEditPaste()) {
            // It's important to use the page transition from the suggestion or we might end
            // up saving generated URLs as typed URLs, which would then pollute the subsequent
            // omnibox results. There is one special case where the suggestion text was pasted,
            // where we want the transition type to be LINK.

            transition = PageTransition.LINK;
        }
        mDelegate.loadUrl(url, transition, inputStart);
    }

    /**
     * Make a zero suggest request if:
     * - Native is loaded.
     * - The URL bar has focus.
     * - The current tab is not incognito.
     */
    private void startZeroSuggest() {
        // hasWindowFocus() can return true before onWindowFocusChanged has been called, so this
        // is an optimization, but not entirely reliable.  The underlying controller needs to also
        // ensure we do not double trigger zero query.
        if (!mParent.hasWindowFocus()) return;

        // Reset "edited" state in the omnibox if zero suggest is triggered -- new edits
        // now count as a new session.
        mHasStartedNewOmniboxEditSession = false;
        mNewOmniboxEditSessionTimestamp = -1;
        if (mNativeInitialized && mDelegate.isUrlBarFocused() && mToolbarDataProvider.hasTab()) {
            mAutocomplete.startZeroSuggest(mToolbarDataProvider.getProfile(),
                    mUrlBarEditingTextProvider.getTextWithAutocomplete(),
                    mToolbarDataProvider.getCurrentUrl(), mToolbarDataProvider.getTitle(),
                    mDelegate.didFocusUrlFromFakebox());
        }
    }

    /**
     * Sets to show cached zero suggest results. This will start both caching zero suggest results
     * in shared preferences and also attempt to show them when appropriate without needing native
     * initialization. See {@link #showCachedZeroSuggestResultsIfAvailable()} for
     * showing the loaded results before native initialization.
     * @param showCachedZeroSuggestResults Whether cached zero suggest should be shown.
     */
    public void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults) {
        mShowCachedZeroSuggestResults = showCachedZeroSuggestResults;
        if (mShowCachedZeroSuggestResults) mAutocomplete.startCachedZeroSuggest();
    }

    /**
     * Signals the omnibox to shows the cached zero suggest results if they have been loaded from
     * cache successfully.
     */
    public void showCachedZeroSuggestResultsIfAvailable() {
        if (!mShowCachedZeroSuggestResults || mSuggestionList == null) return;
        setSuggestionsListVisibility(true);
    }

    /**
     * Update the visuals of the autocomplete UI.
     * @param useDarkColors Whether dark colors should be applied to the UI.
     */
    public void updateVisualsForState(boolean useDarkColors) {
        if (mSuggestionList != null) {
            mSuggestionList.refreshPopupBackground();
        }
        mSuggestionListAdapter.setUseDarkColors(useDarkColors);
    }

    private void clearSuggestions(boolean notifyChange) {
        mSuggestionItems.clear();
        // Make sure to notify the adapter. If the ListView becomes out of sync
        // with its adapter and it has not been notified, it will throw an
        // exception when some UI events are propagated.
        if (notifyChange) mSuggestionListAdapter.notifyDataSetChanged();
    }

    /**
     * Hides the omnibox suggestion popup.
     *
     * <p>
     * Signals the autocomplete controller to stop generating omnibox suggestions.
     *
     * @see AutocompleteController#stop(boolean)
     */
    private void hideSuggestions() {
        if (mAutocomplete == null || !mNativeInitialized) return;

        if (mShowSuggestions != null) mParent.removeCallbacks(mShowSuggestions);

        stopAutocomplete(true);

        setSuggestionsListVisibility(false);
        clearSuggestions(true);
    }

    /**
     * Signals the autocomplete controller to stop generating omnibox suggestions and cancels the
     * queued task to start the autocomplete controller, if any.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    private void stopAutocomplete(boolean clear) {
        if (mAutocomplete != null) mAutocomplete.stop(clear);
        cancelPendingAutocompleteStart();
    }

    /**
     * Cancels the queued task to start the autocomplete controller, if any.
     */
    @VisibleForTesting
    public void cancelPendingAutocompleteStart() {
        if (mRequestSuggestions != null) {
            // There is a request for suggestions either waiting for the native side
            // to start, or on the message queue. Remove it from wherever it is.
            if (!mDeferredNativeRunnables.remove(mRequestSuggestions)) {
                mParent.removeCallbacks(mRequestSuggestions);
            }
            mRequestSuggestions = null;
        }
    }

    /**
     * Trigger autocomplete for the given query.
     */
    public void startAutocompleteForQuery(String query) {
        stopAutocomplete(false);
        if (mToolbarDataProvider.hasTab()) {
            mAutocomplete.start(mToolbarDataProvider.getProfile(),
                    mToolbarDataProvider.getCurrentUrl(), query, -1, false, false);
        }
    }

    /**
     * Sets the autocomplete controller for the location bar.
     *
     * @param controller The controller that will handle autocomplete/omnibox suggestions.
     * @note Only used for testing.
     */
    @VisibleForTesting
    public void setAutocompleteController(AutocompleteController controller) {
        if (mAutocomplete != null) stopAutocomplete(true);
        mAutocomplete = controller;
    }

    private static abstract class DeferredOnSelectionRunnable implements Runnable {
        protected final OmniboxSuggestion mSuggestion;
        protected final int mPosition;
        protected boolean mShouldLog;

        public DeferredOnSelectionRunnable(OmniboxSuggestion suggestion, int position) {
            this.mSuggestion = suggestion;
            this.mPosition = position;
        }

        /**
         * Set whether the selection matches with native results for logging to make sense.
         * @param log Whether the selection should be logged in native code.
         */
        public void setShouldLog(boolean log) {
            mShouldLog = log;
        }

        /**
         * @return Whether the selection should be logged in native code.
         */
        public boolean shouldLog() {
            return mShouldLog;
        }
    }
}
