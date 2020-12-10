// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
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
class LocationBarMediator implements LocationBarDataProvider.Observer, FakeboxDelegate,
                                     VoiceRecognitionHandler.Delegate,
                                     AssistantVoiceSearchService.Observer, UrlBarDelegate,
                                     OnKeyListener {
    private final LocationBarLayout mLocationBarLayout;
    private VoiceRecognitionHandler mVoiceRecognitionHandler;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private AssistantVoiceSearchService mAssistantVoiceSearchService;
    private final OneshotSupplierImpl<AssistantVoiceSearchService> mAssistantVoiceSearchSupplier;
    private StatusCoordinator mStatusCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private OmniboxPrerender mOmniboxPrerender;
    private UrlBarCoordinator mUrlCoordinator;
    private ObservableSupplier<Profile> mProfileSupplier;
    private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    private CallbackController mCallbackController = new CallbackController();
    private final OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    private final LocaleManager mLocaleManager;

    private boolean mNativeInitialized;

    /*package */ LocationBarMediator(@NonNull LocationBarLayout locationBarLayout,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull OneshotSupplierImpl<AssistantVoiceSearchService> assistantVoiceSearchSupplier,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull PrivacyPreferencesManagerImpl privacyPreferencesManager,
            @NonNull OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            @NonNull LocaleManager localeManager) {
        mLocationBarLayout = locationBarLayout;
        mLocationBarDataProvider = locationBarDataProvider;
        mLocationBarDataProvider.addObserver(this);
        mAssistantVoiceSearchSupplier = assistantVoiceSearchSupplier;
        mVoiceRecognitionHandler = new VoiceRecognitionHandler(this, mAssistantVoiceSearchSupplier);
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(mCallbackController.makeCancelable(this::setProfile));
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mOverrideUrlLoadingDelegate = overrideUrlLoadingDelegate;
        mLocaleManager = localeManager;
    }

    /**
     * Sets coordinators post-construction; they can't be set at construction time since
     * LocationBarMediator is a delegate for them, so is constructed beforehand.
     *
     * @param urlCoordinator Coordinator for the url bar.
     * @param autocompleteCoordinator Coordinator for the autocomplete component.
     * @param statusCoordinator Coordinator for the status icon.
     */
    /*package */ void setCoordinators(UrlBarCoordinator urlCoordinator,
            AutocompleteCoordinator autocompleteCoordinator, StatusCoordinator statusCoordinator) {
        mUrlCoordinator = urlCoordinator;
        mAutocompleteCoordinator = autocompleteCoordinator;
        mStatusCoordinator = statusCoordinator;
    }

    /*package */ void destroy() {
        if (mAssistantVoiceSearchService != null) {
            mAssistantVoiceSearchService.destroy();
            mAssistantVoiceSearchService = null;
        }
        mStatusCoordinator = null;
        mAutocompleteCoordinator = null;
        mUrlCoordinator = null;
        mPrivacyPreferencesManager = null;
    }

    /*package */ void onUrlFocusChange(boolean hasFocus) {
        mLocationBarLayout.onUrlFocusChange(hasFocus);
    }

    /*package */ void onFinishNativeInitialization() {
        mNativeInitialized = true;
        mOmniboxPrerender = new OmniboxPrerender();
        Context context = mLocationBarLayout.getContext();
        mAssistantVoiceSearchService = new AssistantVoiceSearchService(context,
                AppHooks.get().getExternalAuthUtils(), TemplateUrlServiceFactory.get(),
                GSAState.getInstance(context), this, SharedPreferencesManager.getInstance());
        mAssistantVoiceSearchSupplier.set(mAssistantVoiceSearchService);
        mLocationBarLayout.onFinishNativeInitialization();
        setProfile(mProfileSupplier.get());
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

    /*package */ void updateVisualsForState() {
        mLocationBarLayout.onPrimaryColorChanged();
    }

    /*package */ void setShowTitle(boolean showTitle) {
        // This method is only used in CustomTabToolbar.
    }

    /*package */ void updateLoadingState(boolean updateUrl) {
        mLocationBarLayout.updateLoadingState(updateUrl);
    }

    /*package */ void showUrlBarCursorWithoutFocusAnimations() {
        mLocationBarLayout.showUrlBarCursorWithoutFocusAnimations();
    }

    /*package */ void revertChanges() {
        if (mLocationBarLayout.isUrlBarFocused()) {
            String currentUrl = mLocationBarDataProvider.getCurrentUrl();
            if (NativePageFactory.isNativePageUrl(
                        currentUrl, mLocationBarDataProvider.isIncognito())) {
                mLocationBarLayout.setUrlBarTextEmpty();
            } else {
                mLocationBarLayout.setUrlBarText(mLocationBarDataProvider.getUrlBarData(),
                        UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
            }
            mUrlCoordinator.setKeyboardVisibility(false, false);
        } else {
            mLocationBarLayout.setUrl(mLocationBarDataProvider.getCurrentUrl());
        }
    }

    /*package */ void updateStatusIcon() {
        mLocationBarLayout.updateStatusIcon();
    }

    /* package */ void onUrlTextChanged() {
        mLocationBarLayout.onUrlTextChanged();
    }

    /* package */ void onSuggestionsChanged(String autocompleteText, boolean defaultMatchIsSearch) {
        mLocationBarLayout.onSuggestionsChanged();
        // TODO (https://crbug.com/1152501): Refactor the LBM/LBC relationship such that LBM doesn't
        // need to communicate with other coordinators like this.
        mStatusCoordinator.onDefaultMatchClassified(defaultMatchIsSearch);
        String userText = mUrlCoordinator.getTextWithoutAutocomplete();
        if (mUrlCoordinator.shouldAutocomplete()) {
            mUrlCoordinator.setAutocompleteText(userText, autocompleteText);
        }

        mLocationBarLayout.onSuggestionsChanged();
        if (mNativeInitialized
                && !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_INSTANT)
                && mPrivacyPreferencesManager.shouldPrerender()
                && mLocationBarDataProvider.hasTab()) {
            mOmniboxPrerender.prerenderMaybe(userText, mLocationBarLayout.getOriginalUrl(),
                    mAutocompleteCoordinator.getCurrentNativeAutocompleteResult(),
                    mProfileSupplier.get(), mLocationBarDataProvider.getTab());
        }
    }

    /* package */ void onSuggestionsHidden() {
        mLocationBarLayout.onSuggestionsHidden();
    }

    /* package */ void loadUrl(String url, int transition, long inputStart) {
        loadUrlWithPostData(url, transition, inputStart, null, null);
    }

    /* package */ void loadUrlWithPostData(
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

    /* package */ boolean didFocusUrlFromFakebox() {
        return mLocationBarLayout.didFocusUrlFromFakebox();
    }

    /* package */ boolean didFocusUrlFromQueryTiles() {
        return mLocationBarLayout.didFocusUrlFromQueryTiles();
    }

    /* package */ void updateButtonVisibility() {
        mLocationBarLayout.updateButtonVisibility();
    }

    // Private methods

    private void setProfile(Profile profile) {
        if (profile == null || !mNativeInitialized) return;
        mAutocompleteCoordinator.setAutocompleteProfile(profile);
        mOmniboxPrerender.initializeForProfile(profile);

        mLocationBarLayout.setShowIconsWhenUrlFocused(
                SearchEngineLogoUtils.shouldShowSearchEngineLogo(profile.isOffTheRecord()));
    }

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
        // Profile may be null if switching to a tab that has not yet been initialized.
        Profile profile = mProfileSupplier.get();
        if (profile != null && mOmniboxPrerender != null) mOmniboxPrerender.clear(profile);
    }

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

    @Override
    public boolean isUrlBarFocused() {
        return mLocationBarLayout.isUrlBarFocused();
    }

    @Override
    public void clearOmniboxFocus() {
        mLocationBarLayout.clearOmniboxFocus();
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
        assert mLocationBarDataProvider != null;
        Tab tab = mLocationBarDataProvider.getTab();
        return tab == null ? null : tab.getView();
    }

    @Override
    public boolean allowKeyboardLearning() {
        assert mLocationBarDataProvider != null;
        return !mLocationBarDataProvider.isIncognito();
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

    // OnKeyListener implementation.
    @Override
    public boolean onKey(View view, int keyCode, KeyEvent event) {
        boolean isRtl = view.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
        if (mAutocompleteCoordinator.handleKeyEvent(keyCode, event)) {
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (KeyNavigationUtil.isActionDown(event) && event.getRepeatCount() == 0) {
                // Tell the framework to start tracking this event.
                mLocationBarLayout.getKeyDispatcherState().startTracking(event, this);
                return true;
            } else if (KeyNavigationUtil.isActionUp(event)) {
                mLocationBarLayout.getKeyDispatcherState().handleUpEvent(event);
                if (event.isTracking() && !event.isCanceled()) {
                    mLocationBarLayout.backKeyPressed();
                    return true;
                }
            }
        } else if (keyCode == KeyEvent.KEYCODE_ESCAPE) {
            if (KeyNavigationUtil.isActionDown(event) && event.getRepeatCount() == 0) {
                revertChanges();
                return true;
            }
        } else if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            // Ensures URL bar doesn't lose focus, when RIGHT or LEFT (RTL) key is pressed while
            // the cursor is positioned at the end of the text.
            TextView tv = (TextView) view;
            return tv.getSelectionStart() == tv.getSelectionEnd()
                    && tv.getSelectionEnd() == tv.getText().length();
        }
        return false;
    }
}
