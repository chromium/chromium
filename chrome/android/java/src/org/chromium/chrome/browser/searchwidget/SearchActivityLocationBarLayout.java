// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;

/** Implementation of the {@link LocationBarLayout} that is displayed for widget searches. */
public class SearchActivityLocationBarLayout extends LocationBarLayout {
    /** Delegates calls out to the containing Activity. */
    public static interface Delegate {
        /** Load a URL in the associated tab. */
        void loadUrl(String url);

        /** The user hit the back button. */
        void backKeyPressed();
    }

    private Delegate mDelegate;
    private boolean mPendingSearchPromoDecision;
    private boolean mPendingBeginQuery;

    public SearchActivityLocationBarLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.layout.location_bar_base);
        setUrlBarFocusable(true);
        setBackground(ToolbarPhone.createModernLocationBarBackground(getResources()));

        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        getAutocompleteCoordinator().setShouldPreventOmniboxAutocomplete(
                mPendingSearchPromoDecision);
    }

    /** Set the {@link Delegate}. */
    void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void loadUrl(String url, int transition, long inputStart) {
        mDelegate.loadUrl(url);
        LocaleManager.getInstance().recordLocaleBasedSearchMetrics(true, url, transition);
    }

    @Override
    public void backKeyPressed() {
        mDelegate.backKeyPressed();
    }

    @Override
    public void setUrlToPageUrl() {
        // Explicitly do nothing.  The tab is invisible, so showing its URL would be confusing.
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        setAutocompleteProfile(Profile.getLastUsedProfile().getOriginalProfile());

        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        getAutocompleteCoordinator().setShouldPreventOmniboxAutocomplete(
                mPendingSearchPromoDecision);
    }

    /** Called when the SearchActivity has finished initialization. */
    void onDeferredStartup(boolean isVoiceSearchIntent) {
        getAutocompleteCoordinator().prefetchZeroSuggestResults();

        if (mVoiceRecognitionHandler != null) {
            SearchWidgetProvider.updateCachedVoiceSearchAvailability(
                    mVoiceRecognitionHandler.isVoiceSearchEnabled());
        }
        if (isVoiceSearchIntent && mUrlBar.isFocused()) onUrlFocusChange(true);

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
            beginQueryInternal(isVoiceSearchIntent);
            mPendingBeginQuery = false;
        }
    }

    /**
     * Begins a new query.
     * @param isVoiceSearchIntent Whether this is a voice search.
     * @param optionalText Prepopulate with a query, this may be null.
     * */
    void beginQuery(boolean isVoiceSearchIntent, @Nullable String optionalText) {
        // Clear the text regardless of the promo decision.  This allows the user to enter text
        // before native has been initialized and have it not be cleared one the delayed beginQuery
        // logic is performed.
        mUrlCoordinator.setUrlBarData(
                UrlBarData.forNonUrlText(optionalText == null ? "" : optionalText),
                UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);

        if (mPendingSearchPromoDecision) {
            mPendingBeginQuery = true;
            return;
        }

        beginQueryInternal(isVoiceSearchIntent);
    }

    private void beginQueryInternal(boolean isVoiceSearchIntent) {
        assert !mPendingSearchPromoDecision;

        if (mVoiceRecognitionHandler != null && mVoiceRecognitionHandler.isVoiceSearchEnabled()
                && isVoiceSearchIntent) {
            mVoiceRecognitionHandler.startVoiceRecognition(
                    LocationBarVoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);
        } else {
            focusTextBox();
        }
    }

    @Override
    protected void updateButtonVisibility() {
        super.updateButtonVisibility();
        updateMicButtonVisibility(1.0f);
        findViewById(R.id.url_action_container).setVisibility(View.VISIBLE);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        super.onUrlFocusChange(hasFocus);
        if (hasFocus) setUrlFocusChangeInProgress(false);
    }

    // TODO(tedchoc): Investigate focusing regardless of the search promo state and just ensure
    //                we don't start processing non-cached suggestion requests until that state
    //                is finalized after native has been initialized.
    private void focusTextBox() {
        if (!mUrlBar.hasFocus()) mUrlBar.requestFocus();
        getAutocompleteCoordinator().setShowCachedZeroSuggestResults(true);

        new Handler().post(new Runnable() {
            @Override
            public void run() {
                getWindowAndroid().getKeyboardDelegate().showKeyboard(mUrlBar);
            }
        });
    }
}
