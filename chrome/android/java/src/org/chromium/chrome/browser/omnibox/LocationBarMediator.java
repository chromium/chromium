// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.List;

/**
 * Mediator for the LocationBar component. Intended location for LocationBar business logic;
 * currently, migration of this logic out of LocationBarLayout is in progress.
 */
class LocationBarMediator implements LocationBar, LocationBarDataProvider.Observer,
                                     AutocompleteDelegate, FakeboxDelegate,
                                     VoiceRecognitionHandler.Delegate,
                                     AssistantVoiceSearchService.Observer, UrlBarDelegate {
    private final OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    private final LocaleManager mLocaleManager;

    private LocationBarLayout mLocationBarLayout;
    private VoiceRecognitionHandler mVoiceRecognitionHandler;
    private LocationBarDataProvider mLocationBarDataProvider;
    private AssistantVoiceSearchService mAssistantVoiceSearchService;
    private OneshotSupplierImpl<AssistantVoiceSearchService> mAssistantVoiceSearchSupplier;
    private StatusCoordinator mStatusCoordinator;
    private boolean mNativeInitialized;

    /*package */ LocationBarMediator(@NonNull LocationBarLayout locationBarLayout,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull OneshotSupplierImpl<AssistantVoiceSearchService> assistantVoiceSearchSupplier,
            @NonNull OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            @NonNull LocaleManager localeManager) {
        mLocationBarLayout = locationBarLayout;
        mOverrideUrlLoadingDelegate = overrideUrlLoadingDelegate;
        mLocaleManager = localeManager;
        mNativeInitialized = false;

        mLocationBarDataProvider = locationBarDataProvider;
        mLocationBarDataProvider.addObserver(this);

        mAssistantVoiceSearchSupplier = assistantVoiceSearchSupplier;
        mVoiceRecognitionHandler = new VoiceRecognitionHandler(this, mAssistantVoiceSearchSupplier);
    }

    /*package */ void onUrlFocusChange(boolean hasFocus) {
        mLocationBarLayout.onUrlFocusChange(hasFocus);
    }

    /*package */ void onFinishNativeInitialization() {
        mNativeInitialized = true;
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

    // Private methods

    private void focusCurrentTab() {
        assert mLocationBarDataProvider != null;
        if (mLocationBarDataProvider.hasTab()) {
            View view = mLocationBarDataProvider.getTab().getView();
            if (view != null) view.requestFocus();
        }
    }

    // LocationBarData.Observer implementation

    @Override
    public void onTitleChanged() {}

    @Override
    public void onUrlChanged() {
        mLocationBarLayout.setUrl(mLocationBarDataProvider.getCurrentUrl());
    }

    @Override
    public void onIncognitoStateChanged() {
        mLocationBarLayout.updateMicButtonState();
    }

    @Override
    public void onNtpStartedLoading() {
        mLocationBarLayout.onNtpStartedLoading();
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
        mLocationBarLayout.updateVisualsForState();
    }

    @Override
    public void setShowTitle(boolean showTitle) {
        mLocationBarLayout.setShowTitle(showTitle);
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
        if (TextUtils.isEmpty(query)) return;

        String queryUrl = TemplateUrlServiceFactory.get().getUrlForSearchQuery(query, searchParams);

        if (!TextUtils.isEmpty(queryUrl)) {
            loadUrl(queryUrl, PageTransition.GENERATED, 0);
        } else {
            mLocationBarLayout.setSearchQuery(query);
        }
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
    public void loadUrl(String url, @PageTransition int transition, long inputStart) {
        loadUrlWithPostData(url, transition, inputStart, null, null);
    }

    @Override
    public void loadUrlWithPostData(
            String url, int transition, long inputStart, String postDataType, byte[] postData) {
        assert mLocationBarDataProvider != null;
        Tab currentTab = mLocationBarDataProvider.getTab();

        // The code of the rest of this class ensures that this can't be called until the native
        // side is initialized
        assert mNativeInitialized : "Loading URL before native side initialized";

        // TODO(crbug.com/1085812): Should be taking a full loaded LoadUrlParams.
        if (mOverrideUrlLoadingDelegate.willHandleLoadUrlWithPostData(url, transition, postDataType,
                    postData, mLocationBarDataProvider.isIncognito())) {
            return;
        }

        if (currentTab != null
                && (currentTab.isNativePage()
                        || UrlUtilities.isNTPUrl(currentTab.getUrlString()))) {
            NewTabPageUma.recordOmniboxNavigation(url, transition);
            // Passing in an empty string should not do anything unless the user is at the NTP.
            // Since the NTP has no url, pressing enter while clicking on the URL bar should refresh
            // the page as it does when you click and press enter on any other site.
            if (url.isEmpty()) url = currentTab.getUrlString();
        }

        // Loads the |url| in a new tab or the current ContentView and gives focus to the
        // ContentView.
        if (currentTab != null && !url.isEmpty()) {
            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            loadUrlParams.setVerbatimHeaders(GeolocationHeader.getGeoHeader(url, currentTab));
            loadUrlParams.setTransitionType(transition | PageTransition.FROM_ADDRESS_BAR);
            if (inputStart != 0) {
                loadUrlParams.setInputStartTimestamp(inputStart);
            }

            if (!TextUtils.isEmpty(postDataType)) {
                StringBuilder headers = new StringBuilder();
                String prevHeader = loadUrlParams.getVerbatimHeaders();
                if (prevHeader != null && !prevHeader.isEmpty()) {
                    headers.append(prevHeader);
                    headers.append("\r\n");
                }
                loadUrlParams.setExtraHeaders(new HashMap<String, String>() {
                    { put("Content-Type", postDataType); }
                });
                headers.append(loadUrlParams.getExtraHttpRequestHeadersString());
                loadUrlParams.setVerbatimHeaders(headers.toString());
            }

            if (postData != null && postData.length != 0) {
                loadUrlParams.setPostData(ResourceRequestBody.createFromBytes(postData));
            }

            currentTab.loadUrl(loadUrlParams);
            RecordUserAction.record("MobileOmniboxUse");
        }
        mLocaleManager.recordLocaleBasedSearchMetrics(false, url, transition);

        focusCurrentTab();
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
        loadUrl(url, PageTransition.TYPED, 0);
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
        focusCurrentTab();
    }

    @Override
    public void gestureDetected(boolean isLongPress) {
        mLocationBarLayout.gestureDetected(isLongPress);
    }
}
