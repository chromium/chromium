// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Configuration;
import android.view.ActionMode;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * The public API of the location bar component. Location bar responsibilities are:
 * <ul>
 *   <li>Display the current URL.
 *   <li>Display Status.
 *   <li>Handle omnibox input.
 * </ul>
 *
 * <p>The coordinator creates and owns elements within this component.
 */

public final class LocationBarCoordinator implements LocationBar, NativeInitObserver,
                                                     OmniboxSuggestionsDropdownEmbedder,
                                                     AutocompleteDelegate {
    /** Identifies coordinators with methods specific to a device type. */
    public interface SubCoordinator extends Destroyable {}

    private LocationBarLayout mLocationBarLayout;
    @Nullable
    private SubCoordinator mSubCoordinator;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private UrlBarCoordinator mUrlCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private StatusCoordinator mStatusCoordinator;
    private WindowDelegate mWindowDelegate;
    private View mAutocompleteAnchorView;
    private LocationBarMediator mLocationBarMediator;
    private View mUrlBar;
    private final OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier =
            new OneshotSupplierImpl<>();
    private CallbackController mCallbackController = new CallbackController();

    private boolean mNativeInitialized;

    /**
     * Creates {@link LocationBarCoordinator} and its subcoordinator: {@link
     * LocationBarCoordinatorPhone} or {@link LocationBarCoordinatorTablet}, depending on the type
     * of {@code locationBarLayout}; no sub-coordinator is created for other LocationBarLayout
     * subclasses.
     * {@code LocationBarCoordinator} owns the subcoordinator. Destroying the former destroys the
     * latter.
     *
     * @param locationBarLayout Inflated {@link LocationBarLayout}.
     *         {@code LocationBarCoordinator} takes ownership and will destroy this object.
     * @param profileObservableSupplier The supplier of the active profile.
     * @param locationBarDataProvider {@link LocationBarDataProvider} to be used for accessing
     *         Toolbar state.
     * @param actionModeCallback The default callback for text editing action bar to use.
     * @param windowDelegate {@link WindowDelegate} that will provide {@link Window} related info.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param activityTabProvider An {@link ActivityTabProvider} to access the activity's current
     *         tab.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} object.
     * @param shareDelegateSupplier A supplier for {@link ShareDelegate} object.
     * @param incognitoStateProvider An {@link IncognitoStateProvider} to access the current
     *         incognito state.
     * @param activityLifecycleDispatcher Allows observation of the activity state.
     */
    public LocationBarCoordinator(View locationBarLayout, View autocompleteAnchorView,
            ObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider, ActionMode.Callback actionModeCallback,
            WindowDelegate windowDelegate, WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            IncognitoStateProvider incognitoStateProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate) {
        mLocationBarLayout = (LocationBarLayout) locationBarLayout;
        mWindowDelegate = windowDelegate;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mAutocompleteAnchorView = autocompleteAnchorView;

        mUrlBar = mLocationBarLayout.findViewById(R.id.url_bar);
        OneshotSupplierImpl<AssistantVoiceSearchService> assistantVoiceSearchSupplier =
                new OneshotSupplierImpl();
        // TODO(crbug.com/1151513): Inject LocaleManager instance to LocationBarCoordinator instead
        // of using the singleton.
        mLocationBarMediator = new LocationBarMediator(mLocationBarLayout, locationBarDataProvider,
                assistantVoiceSearchSupplier, profileObservableSupplier,
                PrivacyPreferencesManagerImpl.getInstance(), overrideUrlLoadingDelegate,
                LocaleManager.getInstance(), mTemplateUrlServiceSupplier);

        mUrlCoordinator =
                new UrlBarCoordinator((UrlBar) mUrlBar, windowDelegate, actionModeCallback,
                        mCallbackController.makeCancelable(mLocationBarMediator::onUrlFocusChange),
                        mLocationBarMediator, windowAndroid.getKeyboardDelegate());
        mAutocompleteCoordinator = new AutocompleteCoordinator(mLocationBarLayout, this, this,
                mUrlCoordinator, activityLifecycleDispatcher, modalDialogManagerSupplier,
                activityTabProvider, shareDelegateSupplier, locationBarDataProvider);
        StatusView statusView = mLocationBarLayout.findViewById(R.id.location_bar_status);
        mStatusCoordinator = new StatusCoordinator(isTablet(), statusView, mUrlCoordinator,
                incognitoStateProvider, modalDialogManagerSupplier, locationBarDataProvider);
        mLocationBarMediator.setCoordinators(
                mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        mUrlBar.setOnKeyListener(mLocationBarMediator);

        mUrlCoordinator.addUrlTextChangeListener(mAutocompleteCoordinator);

        // The LocationBar's direction is tied to the UrlBar's text direction. Icons inside the
        // location bar, e.g. lock, refresh, X, should be reversed if UrlBar's text is RTL.
        mUrlCoordinator.setUrlDirectionListener(
                mCallbackController.makeCancelable(layoutDirection -> {
                    ViewCompat.setLayoutDirection(mLocationBarLayout, (Integer) layoutDirection);
                    mAutocompleteCoordinator.updateSuggestionListLayoutDirection();
                }));

        mLocationBarLayout.getContext().registerComponentCallbacks(mLocationBarMediator);
        mLocationBarLayout.addUrlFocusChangeListener(mAutocompleteCoordinator);
        mLocationBarLayout.addUrlFocusChangeListener(mUrlCoordinator);
        mLocationBarLayout.initialize(mAutocompleteCoordinator, mUrlCoordinator, mStatusCoordinator,
                locationBarDataProvider, windowDelegate, windowAndroid,
                mLocationBarMediator.getVoiceRecognitionHandler(), assistantVoiceSearchSupplier);

        if (locationBarLayout instanceof LocationBarPhone) {
            mSubCoordinator = new LocationBarCoordinatorPhone(
                    (LocationBarPhone) locationBarLayout, mStatusCoordinator);
        } else if (locationBarLayout instanceof LocationBarTablet) {
            mSubCoordinator =
                    new LocationBarCoordinatorTablet((LocationBarTablet) locationBarLayout);
        }
    }

    @Override
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;
        if (mSubCoordinator != null) {
            mSubCoordinator.destroy();
            mSubCoordinator = null;
        }
        mUrlBar.setOnKeyListener(null);
        mUrlBar = null;
        mUrlCoordinator.destroy();
        mUrlCoordinator = null;
        mLocationBarLayout.getContext().unregisterComponentCallbacks(mLocationBarMediator);
        mLocationBarLayout.removeUrlFocusChangeListener(mAutocompleteCoordinator);
        mAutocompleteCoordinator.destroy();
        mAutocompleteCoordinator = null;
        mStatusCoordinator.destroy();
        mStatusCoordinator = null;
        mLocationBarLayout.destroy();
        mLocationBarLayout = null;
        mCallbackController.destroy();
        mCallbackController = null;
        mLocationBarMediator.destroy();
        mLocationBarMediator = null;
    }

    @Override
    public void onFinishNativeInitialization() {
        mTemplateUrlServiceSupplier.set(TemplateUrlServiceFactory.get());
        mLocationBarMediator.onFinishNativeInitialization();
        mAutocompleteCoordinator.onNativeInitialized();
        mStatusCoordinator.onNativeInitialized();
        mNativeInitialized = true;
    }

    /**
     * Runs logic that can't be invoked until after native is initialized but shouldn't be on the
     * critical path, e.g. pre-fetching autocomplete suggestions. Contrast with
     * {@link #onFinishNativeInitialization}, which is for logic that should be on the critical path
     * and need native to be initialized. This method must be called after
     * onFinishNativeInitialization.
     */
    @Override
    public void onDeferredStartup() {
        assert mNativeInitialized;
        startAutocompletePrefetch();
    }

    @Override
    public void updateVisualsForState() {
        mLocationBarMediator.updateVisualsForState();
    }

    @Override
    public void setShowTitle(boolean showTitle) {
        mLocationBarMediator.setShowTitle(showTitle);
    }

    @Override
    public void showUrlBarCursorWithoutFocusAnimations() {
        mLocationBarMediator.showUrlBarCursorWithoutFocusAnimations();
    }

    @Override
    public void selectAll() {
        mUrlCoordinator.selectAll();
    }

    @Override
    public void revertChanges() {
        mLocationBarMediator.revertChanges();
    }

    @Override
    public View getContainerView() {
        return mLocationBarLayout.getContainerView();
    }

    @Override
    public View getSecurityIconView() {
        return mLocationBarLayout.getSecurityIconView();
    }

    /** Returns the {@link VoiceRecognitionHandler} associated with this LocationBar. */
    @Nullable
    @Override
    public VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mLocationBarMediator.getVoiceRecognitionHandler();
    }

    @Override
    public FakeboxDelegate getFakeboxDelegate() {
        return mLocationBarMediator;
    }

    // OmniboxSuggestionsDropdownEmbedder implementation
    @Override
    public boolean isTablet() {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mLocationBarLayout.getContext());
    }

    @Override
    public WindowDelegate getWindowDelegate() {
        return mWindowDelegate;
    }

    @Override
    public View getAnchorView() {
        return mAutocompleteAnchorView;
    }

    @Override
    public View getAlignmentView() {
        return isTablet() ? mLocationBarLayout : null;
    }

    // AutocompleteDelegate implementation.

    @Override
    public void onUrlTextChanged() {
        mLocationBarMediator.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsChanged(String autocompleteText, boolean defaultMatchIsSearch) {
        mLocationBarMediator.onSuggestionsChanged(autocompleteText, defaultMatchIsSearch);
    }

    @Override
    public void setKeyboardVisibility(boolean shouldShow, boolean delayHide) {
        mUrlCoordinator.setKeyboardVisibility(shouldShow, delayHide);
    }

    @Override
    public boolean isKeyboardActive() {
        return KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                       mLocationBarLayout.getContext(), mUrlBar)
                || (mLocationBarLayout.getContext().getResources().getConfiguration().keyboard
                        == Configuration.KEYBOARD_QWERTY);
    }

    @Override
    public void loadUrl(String url, int transition, long inputStart) {
        mLocationBarMediator.loadUrl(url, transition, inputStart);
    }

    @Override
    public void loadUrlWithPostData(
            String url, int transition, long inputStart, String postDataType, byte[] postData) {
        mLocationBarMediator.loadUrlWithPostData(
                url, transition, inputStart, postDataType, postData);
    }

    @Override
    public boolean didFocusUrlFromFakebox() {
        return mLocationBarMediator.didFocusUrlFromFakebox();
    }

    @Override
    public boolean isUrlBarFocused() {
        return mLocationBarMediator.isUrlBarFocused();
    }

    @Override
    public boolean didFocusUrlFromQueryTiles() {
        return mLocationBarMediator.didFocusUrlFromQueryTiles();
    }

    @Override
    public void clearOmniboxFocus() {
        mLocationBarMediator.clearOmniboxFocus();
    }

    @Override
    public void setOmniboxEditingText(String text) {
        mUrlCoordinator.setUrlBarData(UrlBarData.forNonUrlText(text), UrlBar.ScrollType.NO_SCROLL,
                UrlBarCoordinator.SelectionState.SELECT_END);
        mLocationBarMediator.updateButtonVisibility();
    }

    /**
     * Returns the {@link LocationBarCoordinatorPhone} for this coordinator.
     *
     * @throws ClassCastException if this coordinator holds a {@link SubCoordinator} of a different
     *         type.
     */
    @NonNull
    public LocationBarCoordinatorPhone getPhoneCoordinator() {
        assert mSubCoordinator != null;
        return (LocationBarCoordinatorPhone) mSubCoordinator;
    }

    /**
     * Returns the {@link LocationBarCoordinatorTablet} for this coordinator.
     *
     * @throws ClassCastException if this coordinator holds a {@link SubCoordinator} of a different
     *         type.
     */
    @NonNull
    public LocationBarCoordinatorTablet getTabletCoordinator() {
        assert mSubCoordinator != null;
        return (LocationBarCoordinatorTablet) mSubCoordinator;
    }

    /** Initiates a pre-fetch of autocomplete suggestions. */
    public void startAutocompletePrefetch() {
        if (!mNativeInitialized) return;
        mAutocompleteCoordinator.prefetchZeroSuggestResults();
    }

    /**
     * Updates progress of current the URL focus change animation.
     *
     * @param fraction 1.0 is 100% focused, 0 is completely unfocused.
     */
    public void setUrlFocusChangeFraction(float fraction) {
        mLocationBarMediator.setUrlFocusChangeFraction(fraction);
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     *
     * <p>Immediately after the animation to transition the URL bar from focused to unfocused
     * finishes, the layout width returned from #getMeasuredWidth() can differ from the final
     * unfocused width (e.g. this value) until the next layout pass is complete.
     *
     * <p>This value may be used to determine whether optional child views should be visible in the
     * unfocused location bar.
     *
     * @param unfocusedWidth The unfocused location bar width.
     */
    public void setUnfocusedWidth(int unfocusedWidth) {
        mLocationBarMediator.setUnfocusedWidth(unfocusedWidth);
    }

    /** Returns the {@link StatusCoordinator} for the LocationBar. */
    public StatusCoordinator getStatusCoordinator() {
        return mStatusCoordinator;
    }

    /**
     * @param focusable Whether the url bar should be focusable.
     */
    public void setUrlBarFocusable(boolean focusable) {
        mUrlCoordinator.setAllowFocus(focusable);
    }

    public void setVoiceRecognitionHandlerForTesting(
            VoiceRecognitionHandler voiceRecognitionHandler) {
        mLocationBarMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
    }

    /* package */ LocationBarMediator getMediatorForTesting() {
        return mLocationBarMediator;
    }
}
