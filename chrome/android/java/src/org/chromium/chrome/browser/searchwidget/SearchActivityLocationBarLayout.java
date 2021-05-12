// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;

/** Implementation of the {@link LocationBarLayout} that is displayed for widget searches. */
public class SearchActivityLocationBarLayout extends LocationBarLayout {
    private boolean mPendingSearchPromoDecision;
    private boolean mPendingBeginQuery;
    private boolean mHasWindowFocus;
    private boolean mUrlBarFocusRequested;

    public SearchActivityLocationBarLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.layout.location_bar_base);
        setBackground(ToolbarPhone.createModernLocationBarBackground(getResources()));
    }

    @Override
    public void initialize(@NonNull AutocompleteCoordinator autocompleteCoordinator,
            @NonNull UrlBarCoordinator urlCoordinator, @NonNull StatusCoordinator statusCoordinator,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull SearchEngineLogoUtils searchEngineLogoUtils) {
        super.initialize(autocompleteCoordinator, urlCoordinator, statusCoordinator,
                locationBarDataProvider, searchEngineLogoUtils);
        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        getAutocompleteCoordinator().setShouldPreventOmniboxAutocomplete(
                mPendingSearchPromoDecision);
        findViewById(R.id.url_action_container).setVisibility(View.VISIBLE);
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();

        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        getAutocompleteCoordinator().setShouldPreventOmniboxAutocomplete(
                mPendingSearchPromoDecision);
    }

    /** Called when the SearchActivity has finished initialization. */
    void onDeferredStartup(
            boolean isVoiceSearchIntent, @NonNull VoiceRecognitionHandler voiceRecognitionHandler) {
        getAutocompleteCoordinator().prefetchZeroSuggestResults();

        SearchWidgetProvider.updateCachedVoiceSearchAvailability(
                voiceRecognitionHandler.isVoiceSearchEnabled());

        assert !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mPendingSearchPromoDecision = false;
        getAutocompleteCoordinator().setShouldPreventOmniboxAutocomplete(
                mPendingSearchPromoDecision);
        String textWithAutocomplete = mUrlCoordinator.getTextWithAutocomplete();
        if (!TextUtils.isEmpty(textWithAutocomplete)) {
            mAutocompleteCoordinator.onTextChanged(
                    mUrlCoordinator.getTextWithoutAutocomplete(), textWithAutocomplete);
        }

        if (mPendingBeginQuery) {
            beginQueryInternal(isVoiceSearchIntent, voiceRecognitionHandler);
            mPendingBeginQuery = false;
        }
    }

    /**
     * Begins a new query.
     * @param isVoiceSearchIntent Whether this is a voice search.
     * @param optionalText Prepopulate with a query, this may be null.
     */
    @VisibleForTesting
    public void beginQuery(boolean isVoiceSearchIntent, @Nullable String optionalText,
            @NonNull VoiceRecognitionHandler voiceRecognitionHandler) {
        // Clear the text regardless of the promo decision.  This allows the user to enter text
        // before native has been initialized and have it not be cleared one the delayed beginQuery
        // logic is performed.
        mUrlCoordinator.setUrlBarData(
                UrlBarData.forNonUrlText(optionalText == null ? "" : optionalText),
                UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);

        if (mPendingSearchPromoDecision || (isVoiceSearchIntent && !mNativeInitialized)) {
            mPendingBeginQuery = true;
            return;
        }

        beginQueryInternal(isVoiceSearchIntent, voiceRecognitionHandler);
    }

    private void beginQueryInternal(
            boolean isVoiceSearchIntent, @NonNull VoiceRecognitionHandler voiceRecognitionHandler) {
        assert !mPendingSearchPromoDecision;
        assert !isVoiceSearchIntent || mNativeInitialized;

        if (voiceRecognitionHandler.isVoiceSearchEnabled() && isVoiceSearchIntent) {
            voiceRecognitionHandler.startVoiceRecognition(
                    VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);
        } else {
            focusTextBox();
        }
    }

    // TODO(tedchoc): Investigate focusing regardless of the search promo state and just ensure
    //                we don't start processing non-cached suggestion requests until that state
    //                is finalized after native has been initialized.
    private void focusTextBox() {
        mUrlBarFocusRequested |= !mUrlBar.hasFocus();
        ensureUrlBarFocusedAndTriggerZeroSuggest();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        mHasWindowFocus = hasFocus;
        if (hasFocus) {
            ensureUrlBarFocusedAndTriggerZeroSuggest();
        } else {
            mUrlBar.clearFocus();
        }
    }

    /**
     * Since there is a race condition between {@link #focusTextBox()} and {@link
     * #onWindowFocusChanged(boolean)}, if call mUrlBar.requestFocus() before onWindowFocusChanged
     * is called, clipboard data will not been received since receive clipboard data needs focus
     * (https://developer.android.com/reference/android/content/ClipboardManager#getPrimaryClip()).
     *
     * Requesting focus ahead of window activation completion results with inability to call up
     * soft keyboard on an early releases of Android S. The remedy is to defer the focus requests
     * until after Window focus change completes. This is tracked by Android bug http://b/186331446.
     */
    private void ensureUrlBarFocusedAndTriggerZeroSuggest() {
        if (mUrlBarFocusRequested && mHasWindowFocus) {
            mUrlBar.post(() -> {
                mUrlBar.requestFocus();
                mUrlCoordinator.setKeyboardVisibility(true, false);
            });
            mUrlBarFocusRequested = false;
        }
        // Use cached suggestions only if native is not yet ready.
        getAutocompleteCoordinator().setShowCachedZeroSuggestResults(!mNativeInitialized);
    }
}
