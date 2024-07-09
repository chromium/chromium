// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/** Implementation of the {@link LocationBarLayout} that is displayed for widget searches. */
public class SearchActivityLocationBarLayout extends LocationBarLayout {
    private boolean mPendingSearchPromoDecision;
    private boolean mPendingBeginQuery;
    private boolean mInteractionFromWidget;

    public SearchActivityLocationBarLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.layout.location_bar_base);
    }

    @Override
    public void initialize(
            @NonNull AutocompleteCoordinator autocompleteCoordinator,
            @NonNull UrlBarCoordinator urlCoordinator,
            @NonNull StatusCoordinator statusCoordinator,
            @NonNull LocationBarDataProvider locationBarDataProvider) {
        super.initialize(
                autocompleteCoordinator,
                urlCoordinator,
                statusCoordinator,
                locationBarDataProvider);
        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mAutocompleteCoordinator.setShouldPreventOmniboxAutocomplete(mPendingSearchPromoDecision);
        findViewById(R.id.url_action_container).setVisibility(View.VISIBLE);

        GradientDrawable backgroundDrawable =
                ToolbarPhone.createModernLocationBarBackground(getContext());
        backgroundDrawable.setTint(
                ChromeColors.getSurfaceColor(
                        getContext(), R.dimen.omnibox_suggestion_bg_elevation));
        backgroundDrawable.setCornerRadius(
                getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_bg_round_corner_radius));

        setBackground(backgroundDrawable);

        // Expand status view's left and right space, and expand the vertical padding of the
        // location bar to match the expanded interface on the regular omnibox.
        setUrlFocusChangePercent(1f, 1f, /* isUrlFocusChangeInProgress= */ false);
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();

        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mAutocompleteCoordinator.setShouldPreventOmniboxAutocomplete(mPendingSearchPromoDecision);
    }

    /** Called when the SearchActivity has finished initialization. */
    void onDeferredStartup(@SearchType int searchType, @NonNull WindowAndroid windowAndroid) {
        SearchActivityPreferencesManager.updateFeatureAvailability(getContext(), windowAndroid);
        assert !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mPendingSearchPromoDecision = false;
        mAutocompleteCoordinator.setShouldPreventOmniboxAutocomplete(mPendingSearchPromoDecision);
        // Do not prefetch suggestions here; instead, we're asking the server for ZPS directly.
        // Issuing multiple requests would result with only the final one being executed.
        mAutocompleteCoordinator.onTextChanged(mUrlCoordinator.getTextWithoutAutocomplete());

        if (mPendingBeginQuery) {
            beginQueryInternal(searchType, windowAndroid);
            mPendingBeginQuery = false;
        }
    }

    /**
     * Begins a new query.
     *
     * @param origin The SearchActivity requestor.
     * @param searchType The type of search to invoke.
     * @param optionalText Prepopulate with a query, this may be null.
     * @param windowAndroid WindowAndroid context.
     */
    @VisibleForTesting
    public void beginQuery(
            @IntentOrigin int origin,
            @SearchType int searchType,
            @Nullable String optionalText,
            @NonNull WindowAndroid windowAndroid) {

        if (origin == IntentOrigin.CUSTOM_TAB) {
            mUrlBar.setHint(R.string.omnibox_on_cct_empty_hint);
        } else {
            mUrlBar.setHint(R.string.omnibox_empty_hint);
        }
        // Clear the text regardless of the promo decision.  This allows the user to enter text
        // before native has been initialized and have it not be cleared one the delayed beginQuery
        // logic is performed.
        mUrlCoordinator.setUrlBarData(
                UrlBarData.forNonUrlText(optionalText == null ? "" : optionalText),
                UrlBar.ScrollType.NO_SCROLL,
                SelectionState.SELECT_ALL);

        if (mPendingSearchPromoDecision || (searchType != SearchType.TEXT && !mNativeInitialized)) {
            mPendingBeginQuery = true;
            return;
        }

        beginQueryInternal(searchType, windowAndroid);
    }

    private void beginQueryInternal(
            @SearchType int searchType, @NonNull WindowAndroid windowAndroid) {
        assert !mPendingSearchPromoDecision;

        // Update voice and lens eligibility in case anything changed in the process.
        if (mNativeInitialized) {
            SearchActivityPreferencesManager.updateFeatureAvailability(getContext(), windowAndroid);
        }

        mInteractionFromWidget = true;
        if (searchType == SearchType.VOICE) {
            runVoiceSearch();
        } else if (searchType == SearchType.LENS) {
            runGoogleLens();
        }
        requestOmniboxFocus();
        mInteractionFromWidget = false;
    }

    /** Begins a new Voice query. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void runVoiceSearch() {
        View micButton = findViewById(R.id.mic_button);
        if (!micButton.performClick()) {
            // Voice recognition is not available. Fall back to regular text search.
            Toast.makeText(
                            getContext(),
                            R.string.quick_action_search_widget_message_no_voice_search,
                            Toast.LENGTH_LONG)
                    .show();
        }
    }

    /** Begins a new Lens query. */
    private void runGoogleLens() {
        View lensButton = findViewById(R.id.lens_camera_button);
        if (!lensButton.performClick()) {
            Toast.makeText(
                            getContext(),
                            R.string.quick_action_search_widget_message_no_google_lens,
                            Toast.LENGTH_LONG)
                    .show();
        }
    }

    /** Focus the Omnibox and present the cached suggestions. */
    void requestOmniboxFocus() {
        mUrlBar.post(
                () -> {
                    if (mUrlCoordinator == null || mAutocompleteCoordinator == null) {
                        return;
                    }

                    mUrlBar.requestFocus();
                    mUrlCoordinator.setKeyboardVisibility(true, false);
                });
    }

    void clearOmniboxFocus() {
        mUrlBar.post(() -> mUrlBar.clearFocus());
    }

    @Override
    public int getVoiceRecogintionSource() {
        return mInteractionFromWidget
                ? VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET
                : super.getVoiceRecogintionSource();
    }

    @Override
    public int getLensEntryPoint() {
        return mInteractionFromWidget
                ? LensEntryPoint.QUICK_ACTION_SEARCH_WIDGET
                : super.getLensEntryPoint();
    }
}
