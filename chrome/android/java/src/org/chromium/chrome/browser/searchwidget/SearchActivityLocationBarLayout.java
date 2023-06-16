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
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/** Implementation of the {@link LocationBarLayout} that is displayed for widget searches. */
public class SearchActivityLocationBarLayout extends LocationBarLayout {
    private boolean mPendingSearchPromoDecision;
    private boolean mPendingBeginQuery;

    public SearchActivityLocationBarLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.layout.location_bar_base);
    }

    @Override
    public void initialize(@NonNull AutocompleteCoordinator autocompleteCoordinator,
            @NonNull UrlBarCoordinator urlCoordinator, @NonNull StatusCoordinator statusCoordinator,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull SearchEngineLogoUtils searchEngineLogoUtils) {
        super.initialize(autocompleteCoordinator, urlCoordinator, statusCoordinator,
                locationBarDataProvider, searchEngineLogoUtils);
        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mAutocompleteCoordinator.setShouldPreventOmniboxAutocomplete(mPendingSearchPromoDecision);
        findViewById(R.id.url_action_container).setVisibility(View.VISIBLE);

        GradientDrawable backgroundDrawable =
                ToolbarPhone.createModernLocationBarBackground(getContext());
        if (OmniboxFeatures.shouldShowModernizeVisualUpdate(getContext())) {
            backgroundDrawable.setTint(OmniboxFeatures.shouldShowActiveColorOnOmnibox()
                            ? ChromeColors.getSurfaceColor(
                                    getContext(), R.dimen.omnibox_suggestion_bg_elevation)
                            : ChromeColors.getSurfaceColor(getContext(),
                                    R.dimen.omnibox_suggestion_dropdown_bg_elevation));
            if (OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
                backgroundDrawable.setCornerRadius(getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_bg_round_corner_radius));
            }
            setPaddingRelative(
                    getResources().getDimensionPixelSize(R.dimen.location_bar_start_padding_modern),
                    getPaddingTop(), getPaddingEnd(), getPaddingBottom());
        }
        setBackground(backgroundDrawable);

        // Expand status view's left and right space, and expand the vertical padding of the
        // location bar to match the expanded interface on the regular omnibox.
        setUrlFocusChangePercent(1f);
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();

        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mAutocompleteCoordinator.setShouldPreventOmniboxAutocomplete(mPendingSearchPromoDecision);
    }

    /** Called when the SearchActivity has finished initialization. */
    void onDeferredStartup(@SearchType int searchType,
            @NonNull VoiceRecognitionHandler voiceRecognitionHandler,
            @NonNull WindowAndroid windowAndroid) {
        SearchActivityPreferencesManager.updateFeatureAvailability(getContext(), windowAndroid);
        assert !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mPendingSearchPromoDecision = false;
        mAutocompleteCoordinator.setShouldPreventOmniboxAutocomplete(mPendingSearchPromoDecision);
        String textWithAutocomplete = mUrlCoordinator.getTextWithAutocomplete();
        // Do not prefetch suggestions here; instead, we're asking the server for ZPS directly.
        // Issuing multiple requests would result with only the final one being executed.
        mAutocompleteCoordinator.onTextChanged(
                mUrlCoordinator.getTextWithoutAutocomplete(), textWithAutocomplete);

        if (mPendingBeginQuery) {
            beginQueryInternal(searchType, voiceRecognitionHandler, windowAndroid);
            mPendingBeginQuery = false;
        }
    }

    /**
     * Begins a new query.
     * @param searchType The type of search to invoke.
     * @param optionalText Prepopulate with a query, this may be null.
     * @param voiceRecognitionHandler Handler responsible for managing voice searches.
     * @param windowAndroid WindowAndroid context.
     */
    @VisibleForTesting
    public void beginQuery(@SearchType int searchType, @Nullable String optionalText,
            @NonNull VoiceRecognitionHandler voiceRecognitionHandler,
            @NonNull WindowAndroid windowAndroid) {
        // Clear the text regardless of the promo decision.  This allows the user to enter text
        // before native has been initialized and have it not be cleared one the delayed beginQuery
        // logic is performed.
        mUrlCoordinator.setUrlBarData(
                UrlBarData.forNonUrlText(optionalText == null ? "" : optionalText),
                UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);

        if (mPendingSearchPromoDecision || (searchType != SearchType.TEXT && !mNativeInitialized)) {
            mPendingBeginQuery = true;
            return;
        }

        beginQueryInternal(searchType, voiceRecognitionHandler, windowAndroid);
    }

    private void beginQueryInternal(@SearchType int searchType,
            @NonNull VoiceRecognitionHandler voiceRecognitionHandler,
            @NonNull WindowAndroid windowAndroid) {
        assert !mPendingSearchPromoDecision;

        // Update voice and lens eligibility in case anything changed in the process.
        if (mNativeInitialized) {
            SearchActivityPreferencesManager.updateFeatureAvailability(getContext(), windowAndroid);
        }

        if (searchType == SearchType.VOICE) {
            runVoiceSearch(voiceRecognitionHandler);
        } else if (searchType == SearchType.LENS) {
            runGoogleLens(windowAndroid);
        } else {
            focusTextBox();
        }
    }

    /**
     * Begins a new Voice query.
     *
     * @param voiceRecognitionHandler Handler responsible for managing voice searches.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void runVoiceSearch(@NonNull VoiceRecognitionHandler voiceRecognitionHandler) {
        assert mNativeInitialized;
        // Run Voice before focusing the Omnibox. Voice search may trigger omnibox focus as part of
        // its own flow in the event where the input is ambiguous. Focusing the Omnibox early may
        // affect this flow.
        //
        // Note that the Voice search will call us back in the event of any failure via
        // notifyVoiceRecognitionCanceled() call, giving us the opportunity to focus the Omnibox.
        if (voiceRecognitionHandler.isVoiceSearchEnabled()) {
            voiceRecognitionHandler.startVoiceRecognition(
                    VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);
            return;
        }

        // Voice recognition is not available. Fall back to regular text search.
        Toast.makeText(getContext(), R.string.quick_action_search_widget_message_no_voice_search,
                     Toast.LENGTH_LONG)
                .show();
        focusTextBox();
    }

    /**
     * Begins a new Lens query.
     *
     * @param windowAndroid WindowAndroid context.
     */
    private void runGoogleLens(@NonNull WindowAndroid windowAndroid) {
        assert mNativeInitialized;

        // Preemptively focus the Search box to handle fallback to text search for every case where
        // Lens search could not be performed, including events where Lens is started and canceled
        // by the User.
        // Unlike Voice, Lens gives us no feedback about completion and does not interact with the
        // Omnibox at any point. Focus is relevant here, because otherwise canceled Lens intent
        // lands the User on a white, unfocused activity with no keyboard and single, empty text
        // field on top.
        focusTextBox();

        LensController lensController = LensController.getInstance();
        LensQueryParams lensParams =
                new LensQueryParams
                        .Builder(LensEntryPoint.QUICK_ACTION_SEARCH_WIDGET,
                                mLocationBarDataProvider.isIncognito(),
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()))
                        .build();
        if (lensController.isLensEnabled(lensParams)) {
            lensController.startLens(windowAndroid,
                    new LensIntentParams
                            .Builder(LensEntryPoint.QUICK_ACTION_SEARCH_WIDGET,
                                    mLocationBarDataProvider.isIncognito())
                            .build());
            return;
        }

        Toast.makeText(getContext(), R.string.quick_action_search_widget_message_no_google_lens,
                     Toast.LENGTH_LONG)
                .show();
        // No need to focus, because the Text field should already be focused.
    }

    /**
     * Focus the Omnibox and present the cached suggestions.
     */
    void focusTextBox() {
        mUrlBar.post(() -> {
            if (mUrlCoordinator == null || mAutocompleteCoordinator == null) {
                return;
            }

            mUrlBar.requestFocus();
            mUrlCoordinator.setKeyboardVisibility(true, false);
            mAutocompleteCoordinator.startCachedZeroSuggest();
        });
    }

    @Override
    public void notifyVoiceRecognitionCanceled() {
        focusTextBox();
    }
}
