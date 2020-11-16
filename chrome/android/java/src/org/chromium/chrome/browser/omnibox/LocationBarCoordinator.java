// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.ActionMode;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider.Observer;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

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
public final class LocationBarCoordinator implements LocationBar, FakeboxDelegate, UrlBarDelegate,
                                                     NativeInitObserver, Observer,
                                                     OmniboxSuggestionsDropdownEmbedder {
    /** Identifies coordinators with methods specific to a device type. */
    public interface SubCoordinator extends Destroyable {}

    private LocationBarLayout mLocationBarLayout;
    @Nullable
    private SubCoordinator mSubCoordinator;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private LocationBarDataProvider mLocationbarDataProvider;
    private UrlBarCoordinator mUrlCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private StatusCoordinator mStatusCoordinator;
    private WindowDelegate mWindowDelegate;
    private View mAutocompleteAnchorView;
    private CallbackController mCallbackController = new CallbackController();

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
        mLocationbarDataProvider = locationBarDataProvider;
        mLocationbarDataProvider.addObserver(this);
        mAutocompleteAnchorView = autocompleteAnchorView;

        if (locationBarLayout instanceof LocationBarPhone) {
            mSubCoordinator = new LocationBarCoordinatorPhone((LocationBarPhone) locationBarLayout);
        } else if (locationBarLayout instanceof LocationBarTablet) {
            mSubCoordinator =
                    new LocationBarCoordinatorTablet((LocationBarTablet) locationBarLayout);
        }

        View urlBar = mLocationBarLayout.findViewById(R.id.url_bar);
        mUrlCoordinator =
                new UrlBarCoordinator((UrlBar) urlBar, windowDelegate, actionModeCallback);
        mAutocompleteCoordinator =
                new AutocompleteCoordinator(mLocationBarLayout, mLocationBarLayout, this,
                        mUrlCoordinator, activityLifecycleDispatcher, modalDialogManagerSupplier,
                        activityTabProvider, shareDelegateSupplier, locationBarDataProvider);
        StatusView statusView = mLocationBarLayout.findViewById(R.id.location_bar_status);
        mStatusCoordinator = new StatusCoordinator(isTablet(), statusView, mUrlCoordinator,
                incognitoStateProvider, modalDialogManagerSupplier, locationBarDataProvider);

        mUrlCoordinator.setDelegate(this);
        mUrlCoordinator.addUrlTextChangeListener(mAutocompleteCoordinator);
        mUrlCoordinator.addUrlTextChangeListener(mStatusCoordinator);
        mUrlCoordinator.setOnFocusChangedCallback(mLocationBarLayout::onUrlFocusChange);

        // The LocationBar's direction is tied to the UrlBar's text direction. Icons inside the
        // location bar, e.g. lock, refresh, X, should be reversed if UrlBar's text is RTL.
        mUrlCoordinator.setUrlDirectionListener(
                mCallbackController.makeCancelable(layoutDirection -> {
                    ViewCompat.setLayoutDirection(mLocationBarLayout, (Integer) layoutDirection);
                    mAutocompleteCoordinator.updateSuggestionListLayoutDirection();
                }));

        mLocationBarLayout.addUrlFocusChangeListener(mAutocompleteCoordinator);
        mLocationBarLayout.initialize(mAutocompleteCoordinator, mUrlCoordinator, mStatusCoordinator,
                locationBarDataProvider, profileObservableSupplier, windowDelegate, windowAndroid,
                overrideUrlLoadingDelegate);
    }

    @Override
    public void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        if (mSubCoordinator != null) {
            mSubCoordinator.destroy();
            mSubCoordinator = null;
        }
        if (mUrlCoordinator != null) {
            mUrlCoordinator = null;
        }
        if (mAutocompleteCoordinator != null) {
            if (mLocationBarLayout != null) {
                mLocationBarLayout.removeUrlFocusChangeListener(mAutocompleteCoordinator);
            }
            mAutocompleteCoordinator.destroy();
            mAutocompleteCoordinator = null;
        }
        if (mStatusCoordinator != null) {
            mStatusCoordinator = null;
        }
        if (mLocationBarLayout != null) {
            mLocationBarLayout.destroy();
            mLocationBarLayout = null;
        }
        if (mLocationbarDataProvider != null) {
            mLocationbarDataProvider.removeObserver(this);
            mLocationbarDataProvider = null;
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        mLocationBarLayout.onFinishNativeInitialization();
    }

    @Override
    public void onDeferredStartup() {
        mLocationBarLayout.onDeferredStartup();
    }

    @Override
    public void onTabLoadingNTP(NewTabPage ntp) {
        mLocationBarLayout.onTabLoadingNTP(ntp);
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

    @Override
    public void updateMicButtonState() {
        mLocationBarLayout.updateMicButtonState();
    }

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

    @Override
    public void setUrlBarFocus(boolean shouldBeFocused, @Nullable String pastedText, int reason) {
        mLocationBarLayout.setUrlBarFocus(shouldBeFocused, pastedText, reason);
    }

    @Override
    public void performSearchQuery(String query, List<String> searchParams) {
        mLocationBarLayout.performSearchQuery(query, searchParams);
    }

    @Override
    public boolean isUrlBarFocused() {
        return mLocationBarLayout.isUrlBarFocused();
    }

    @Nullable
    @Override
    public VoiceRecognitionHandler getVoiceRecognitionHandler() {
        // TODO(crbug.com/1140333): StartSurfaceMediator can call this method after destroy().
        if (mLocationBarLayout == null) {
            return null;
        }
        return mLocationBarLayout.getVoiceRecognitionHandler();
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
    public FakeboxDelegate getFakeboxDelegate() {
        return this;
    }

    // LocationBarDataObserver implementation
    @Override
    public void onTitleChanged() {}

    @Override
    public void onUrlChanged() {
        mLocationBarLayout.setUrl(mLocationbarDataProvider.getCurrentUrl());
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
        mLocationBarLayout.startPrefetch();
    }

    /**
     * Updates progress of current the URL focus change animation.
     *
     * @param fraction 1.0 is 100% focused, 0 is completely unfocused.
     */
    public void setUrlFocusChangeFraction(float fraction) {
        mLocationBarLayout.setUrlFocusChangeFraction(fraction);
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
        mLocationBarLayout.setUnfocusedWidth(unfocusedWidth);
    }
}
