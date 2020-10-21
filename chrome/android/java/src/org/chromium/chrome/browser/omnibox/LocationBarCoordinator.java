// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
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
public final class LocationBarCoordinator implements LocationBar {
    /** Identifies coordinators with methods specific to a device type. */
    public interface SubCoordinator extends Destroyable {}

    private LocationBarLayout mLocationBarLayout;
    @Nullable
    private SubCoordinator mSubCoordinator;

    /**
     * Creates {@link LocationBarCoordinator} and its subcoordinator: {@link
     * LocationBarCoordinatorPhone} or {@link LocationBarCoordinatorTablet}, depending on the type
     * of {@code locationBarLayout}.
     * {@code LocationBarCoordinator} owns the subcoordinator. Destroying the former destroys the
     * latter.
     *
     * @param locationBarLayout Inflated {@link LocationBarPhone} or {@link LocationBarTablet}.
     *         {@code LocationBarCoordinator} takes ownership and will destroy this object.
     * @param profileObservableSupplier The supplier of the active profile.
     * @param toolbarDataProvider {@link ToolbarDataProvider} to be used for accessing Toolbar
     *         state.
     * @param actionModeCallback The default callback for text editing action bar to use.
     * @param windowDelegate {@link WindowDelegate} that will provide {@link Window} related info.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param activityTabProvider An {@link ActivityTabProvider} to access the activity's current
     *         tab.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} object.
     * @param shareDelegateSupplier A supplier for {@link ShareDelegate} object.
     * @param incognitoStateProvider An {@link IncognitoStateProvider} to access the current
     *         incognito state.
     * @throws IllegalArgumentException if the view is neither {@link LocationBarPhone} nor {@link
     *         LocationBarTablet}.
     */
    public LocationBarCoordinator(View locationBarLayout,
            ObservableSupplier<Profile> profileObservableSupplier,
            ToolbarDataProvider toolbarDataProvider, ToolbarActionModeCallback actionModeCallback,
            WindowDelegate windowDelegate, WindowAndroid windowAndroid,
            ActivityTabProvider activityTabProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            IncognitoStateProvider incognitoStateProvider) {
        mLocationBarLayout = (LocationBarLayout) locationBarLayout;

        if (locationBarLayout instanceof LocationBarPhone) {
            mSubCoordinator = new LocationBarCoordinatorPhone((LocationBarPhone) locationBarLayout);
        } else if (locationBarLayout instanceof LocationBarTablet) {
            mSubCoordinator =
                    new LocationBarCoordinatorTablet((LocationBarTablet) locationBarLayout);
        } else {
            assert false : "Expected LocationBarPhone or LocationBarTablet, got "
                           + locationBarLayout.getClass();
            throw new IllegalArgumentException(locationBarLayout.getClass().toString());
        }

        mLocationBarLayout.setToolbarDataProvider(toolbarDataProvider);
        mLocationBarLayout.setProfileSupplier(profileObservableSupplier);
        mLocationBarLayout.setDefaultTextEditActionModeCallback(actionModeCallback);
        mLocationBarLayout.initializeControls(windowDelegate, windowAndroid, activityTabProvider,
                modalDialogManagerSupplier, shareDelegateSupplier, incognitoStateProvider);
    }

    @Override
    public void destroy() {
        if (mSubCoordinator != null) {
            mSubCoordinator.destroy();
            mSubCoordinator = null;
        }
        if (mLocationBarLayout != null) {
            mLocationBarLayout.destroy();
            mLocationBarLayout = null;
        }
    }

    @Override
    public void onDeferredStartup() {
        mLocationBarLayout.onDeferredStartup();
    }

    @Override
    public void onNativeLibraryReady() {
        mLocationBarLayout.onNativeLibraryReady();
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
    public void setUrlToPageUrl() {
        mLocationBarLayout.setUrlToPageUrl();
    }

    @Override
    public void setTitleToPageTitle() {
        mLocationBarLayout.setTitleToPageTitle();
    }

    @Override
    public void setShowTitle(boolean showTitle) {
        mLocationBarLayout.setShowTitle(showTitle);
    }

    @Override
    public void updateLoadingState(boolean updateUrl) {
        mLocationBarLayout.updateLoadingState(updateUrl);
    }

    public ToolbarDataProvider getToolbarDataProvider() {
        return mLocationBarLayout.getToolbarDataProvider();
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
    public boolean shouldForceLTR() {
        return mLocationBarLayout.shouldForceLTR();
    }

    @Override
    public boolean shouldCutCopyVerbatim() {
        return mLocationBarLayout.shouldCutCopyVerbatim();
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

    /** Sets the {@link OverviewModeBehavior}. */
    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mLocationBarLayout.setOverviewModeBehavior(overviewModeBehavior);
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
