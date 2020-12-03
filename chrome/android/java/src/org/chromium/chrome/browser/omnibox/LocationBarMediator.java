// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Mediator for the LocationBar component. Intended location for LocationBar business logic;
 * currently, migration of this logic out of LocationBarLayout is in progress.
 */
class LocationBarMediator implements LocationBar, LocationBarDataProvider.Observer,
                                     AutocompleteDelegate, FakeboxDelegate,
                                     VoiceRecognitionHandler.Delegate,
                                     AssistantVoiceSearchService.Observer, UrlBarDelegate {
    private LocationBarLayout mLocationBarLayout;
    private VoiceRecognitionHandler mVoiceRecognitionHandler;
    private LocationBarDataProvider mLocationBarDataProvider;
    private AssistantVoiceSearchService mAssistantVoiceSearchService;
    private OneshotSupplierImpl<AssistantVoiceSearchService> mAssistantVoiceSearchSupplier;
    private StatusCoordinator mStatusCoordinator;

    /*package */ LocationBarMediator(@NonNull LocationBarLayout locationBarLayout,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull OneshotSupplierImpl<AssistantVoiceSearchService>
                    assistantVoiceSearchSupplier) {
        mLocationBarLayout = locationBarLayout;
        mLocationBarDataProvider = locationBarDataProvider;
        mLocationBarDataProvider.addObserver(this);
        mAssistantVoiceSearchSupplier = assistantVoiceSearchSupplier;
        mVoiceRecognitionHandler = new VoiceRecognitionHandler(this, mAssistantVoiceSearchSupplier);
    }

    /*package */ void onUrlFocusChange(boolean hasFocus) {
        mLocationBarLayout.onUrlFocusChange(hasFocus);
    }

    /*package */ void onFinishNativeInitialization() {
        Context context = mLocationBarLayout.getContext();
        mAssistantVoiceSearchService = new AssistantVoiceSearchService(context,
                AppHooks.get().getExternalAuthUtils(), TemplateUrlServiceFactory.get(),
                GSAState.getInstance(context), this, SharedPreferencesManager.getInstance());
        mAssistantVoiceSearchSupplier.set(mAssistantVoiceSearchService);
        mLocationBarLayout.onFinishNativeInitialization();
    }

    /*package */ void setUrlFocusChangeFraction(float fraction) {
        mLocationBarLayout.setUrlFocusChangeFraction(fraction);
    }

    /*package */ void setUnfocusedWidth(int unfocusedWidth) {
        mLocationBarLayout.setUnfocusedWidth(unfocusedWidth);
    }

    /* package */ void setVoiceRecognitionHandlerForTesting(
            VoiceRecognitionHandler voiceRecognitionHandler) {
        mVoiceRecognitionHandler = voiceRecognitionHandler;
        mLocationBarLayout.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
    }

    /**
     * Sets coordinators post-construction; they can't be set at construction time since
     * LocationBarMediator is a delegate for them, so is constructed beforehand.
     *
     * @param statusCoordinator
     */
    /* package */ void setCoordinators(@NonNull StatusCoordinator statusCoordinator) {
        mStatusCoordinator = statusCoordinator;
    }

    // LocationBarData.Observer implementation
    @Override
    public void onIncognitoStateChanged() {
        mLocationBarLayout.updateMicButtonState();
    }

    @Override
    public void onNtpStartedLoading() {
        mLocationBarLayout.onNtpStartedLoading();
    }

    @Override
    public void onPrimaryColorChanged() {
        mLocationBarLayout.onPrimaryColorChanged();
    }

    @Override
    public void onTitleChanged() {}

    @Override
    public void onUrlChanged() {
        mLocationBarLayout.setUrl(mLocationBarDataProvider.getCurrentUrl());
    }

    // LocationBar implementation.
    @Override
    public void destroy() {
        mLocationBarLayout = null;
        mVoiceRecognitionHandler = null;
        if (mAssistantVoiceSearchService != null) {
            mAssistantVoiceSearchService.destroy();
            mAssistantVoiceSearchService = null;
        }
        mLocationBarDataProvider.removeObserver(this);
        mLocationBarDataProvider = null;
        mStatusCoordinator = null;
    }

    @Override
    public void updateVisualsForState() {
        mLocationBarLayout.onPrimaryColorChanged();
    }

    @Override
    public void setShowTitle(boolean showTitle) {
        // This method is only used in CustomTabToolbar.
    }

    @Override
    public void updateLoadingState(boolean updateUrl) {
        mLocationBarLayout.updateLoadingState(updateUrl);
    }

    @Override
    public void showUrlBarCursorWithoutFocusAnimations() {
        mLocationBarLayout.showUrlBarCursorWithoutFocusAnimations();
    }

    @Override
    public void selectAll() {
        mLocationBarLayout.selectAll();
    }

    @Override
    public void revertChanges() {
        mLocationBarLayout.revertChanges();
    }

    @Override
    public void updateStatusIcon() {
        mLocationBarLayout.updateStatusIcon();
    }

    @Override
    public View getContainerView() {
        return mLocationBarLayout.getContainerView();
    }

    @Override
    public View getSecurityIconView() {
        return mLocationBarLayout.getSecurityIconView();
    }

    // FakeboxDelegate implementation.

    @Override
    public void setUrlBarFocus(boolean shouldBeFocused, @Nullable String pastedText, int reason) {
        mLocationBarLayout.setUrlBarFocus(shouldBeFocused, pastedText, reason);
    }

    @Override
    public void performSearchQuery(String query, List<String> searchParams) {
        mLocationBarLayout.performSearchQuery(query, searchParams);
    }

    @Override
    public @Nullable VoiceRecognitionHandler getVoiceRecognitionHandler() {
        // TODO(crbug.com/1140333): StartSurfaceMediator can call this method after destroy().
        if (mLocationBarLayout == null) {
            return null;
        }

        return mVoiceRecognitionHandler;
    }

    @Override
    public void addUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mLocationBarLayout.addUrlFocusChangeListener(listener);
    }

    @Override
    public void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mLocationBarLayout.removeUrlFocusChangeListener(listener);
    }

    // AutocompleteDelegate implementation.

    @Override
    public void onUrlTextChanged() {
        mLocationBarLayout.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsChanged(String autocompleteText, boolean defaultMatchIsSearch) {
        mLocationBarLayout.onSuggestionsChanged(autocompleteText);
        // TODO (https://crbug.com/1152501): Refactor the LBM/LBC relationship such that LBM doesn't
        // need to communicate with other coordinators like this.
        mStatusCoordinator.onDefaultMatchClassified(defaultMatchIsSearch);
    }

    @Override
    public void onSuggestionsHidden() {
        mLocationBarLayout.onSuggestionsHidden();
    }

    @Override
    public void setKeyboardVisibility(boolean shouldShow, boolean delayHide) {
        mLocationBarLayout.setKeyboardVisibility(shouldShow, delayHide);
    }

    @Override
    public boolean isKeyboardActive() {
        return mLocationBarLayout.isKeyboardActive();
    }

    @Override
    public void loadUrl(String url, int transition, long inputStart) {
        mLocationBarLayout.loadUrl(url, transition, inputStart);
    }

    @Override
    public void loadUrlWithPostData(
            String url, int transition, long inputStart, String postDataType, byte[] postData) {
        mLocationBarLayout.loadUrlWithPostData(url, transition, inputStart, postDataType, postData);
    }

    @Override
    public boolean didFocusUrlFromFakebox() {
        return mLocationBarLayout.didFocusUrlFromFakebox();
    }

    @Override
    public boolean isUrlBarFocused() {
        return mLocationBarLayout.isUrlBarFocused();
    }

    @Override
    public boolean didFocusUrlFromQueryTiles() {
        return mLocationBarLayout.didFocusUrlFromQueryTiles();
    }

    @Override
    public void clearOmniboxFocus() {
        mLocationBarLayout.clearOmniboxFocus();
    }

    @Override
    public void setOmniboxEditingText(String text) {
        mLocationBarLayout.setOmniboxEditingText(text);
    }

    // AssistantVoiceSearchService.Observer implementation.

    @Override
    public void onAssistantVoiceSearchServiceChanged() {
        mLocationBarLayout.onAssistantVoiceSearchServiceChanged();
    }

    // VoiceRecognitionHandler.Delegate implementation.

    @Override
    public void loadUrlFromVoice(String url) {
        mLocationBarLayout.loadUrlFromVoice(url);
    }

    @Override
    public void updateMicButtonState() {
        mLocationBarLayout.updateMicButtonState();
    }

    @Nullable
    @Override
    public FakeboxDelegate getFakeboxDelegate() {
        return null;
    }

    @Override
    public void setSearchQuery(String query) {
        mLocationBarLayout.setSearchQuery(query);
    }

    @Override
    public LocationBarDataProvider getLocationBarDataProvider() {
        return mLocationBarDataProvider;
    }

    @Override
    public AutocompleteCoordinator getAutocompleteCoordinator() {
        return mLocationBarLayout.getAutocompleteCoordinator();
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        return mLocationBarLayout.getWindowAndroid();
    }

    // UrlBarDelegate implementation.

    @Nullable
    @Override
    public View getViewForUrlBackFocus() {
        return mLocationBarLayout.getViewForUrlBackFocus();
    }

    @Override
    public boolean allowKeyboardLearning() {
        return mLocationBarLayout.allowKeyboardLearning();
    }

    @Override
    public void backKeyPressed() {
        mLocationBarLayout.backKeyPressed();
    }

    @Override
    public void gestureDetected(boolean isLongPress) {
        mLocationBarLayout.gestureDetected(isLongPress);
    }
}
