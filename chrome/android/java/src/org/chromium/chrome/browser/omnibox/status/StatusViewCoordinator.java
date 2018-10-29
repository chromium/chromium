// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.chromium.chrome.browser.toolbar.ToolbarPhone.URL_FOCUS_CHANGE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.annotation.DrawableRes;
import android.support.annotation.IntDef;
import android.support.v7.widget.AppCompatImageButton;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusViewCoordinator implements View.OnClickListener {
    /**
     * Specifies the types of buttons shown to signify different types of navigation elements.
     */
    @IntDef({NavigationButtonType.PAGE, NavigationButtonType.MAGNIFIER, NavigationButtonType.EMPTY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationButtonType {
        int PAGE = 0;
        int MAGNIFIER = 1;
        int EMPTY = 2;
    }

    /** Specifies which button should be shown in location bar, if any. */
    @IntDef({LocationBarButtonType.NONE, LocationBarButtonType.SECURITY_ICON,
            LocationBarButtonType.NAVIGATION_ICON})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LocationBarButtonType {
        /** No button should be shown. */
        int NONE = 0;
        /** Security button should be shown (includes offline icon). */
        int SECURITY_ICON = 1;
        /** Navigation button should be shown. */
        int NAVIGATION_ICON = 2;
    }

    /**
     * Delegate interface to provide additional information needed to display this view.
     */
    public interface Delegate {
        /**
         * @return Whether or not to animate icon changes.
         */
        boolean shouldAnimateIconChanges();
    }

    private final Delegate mDelegate;
    private final boolean mIsTablet;
    private final View mParentView;
    private final ImageView mNavigationButton;
    private final AppCompatImageButton mSecurityButton;
    private final TextView mVerboseStatusTextView;

    private final float mUrlMinWidth;
    private final float mVerboseStatusMinWidth;
    private final float mStatusSeparatorWidth;

    private ToolbarDataProvider mToolbarDataProvider;
    private WindowAndroid mWindowAndroid;

    // The type of the navigation button currently showing.
    private @NavigationButtonType int mNavigationButtonType;

    // The type of the security icon currently active.
    @DrawableRes
    private int mSecurityIconResource;

    @LocationBarButtonType
    private int mLocationBarButtonType;

    private AnimatorSet mLocationBarIconActiveAnimator;
    private AnimatorSet mSecurityButtonShowAnimator;
    private AnimatorSet mNavigationIconShowAnimator;

    private boolean mUrlHasFocus;
    private boolean mUseDarkColors;
    private float mUnfocusedLocationBarWidth;
    private int mVerboseStatusTextMaxWidth;
    private boolean mHasSpaceForVerboseStatus;

    /**
     * Creates a new StatusViewCoordinator.
     * @param isTablet Whether the UI is shown on a tablet.
     * @param parentView The parent view that contains the status view, used to retrieve views.
     * @param delegate The delegate that provides additional information needed to display this
     *                 view.
     */
    public StatusViewCoordinator(boolean isTablet, View parentView, Delegate delegate) {
        mIsTablet = isTablet;
        mParentView = parentView;
        mDelegate = delegate;

        mNavigationButton = mParentView.findViewById(R.id.navigation_button);
        assert mNavigationButton != null : "Missing navigation type view.";
        mNavigationButtonType = mIsTablet ? NavigationButtonType.PAGE : NavigationButtonType.EMPTY;

        mSecurityButton = mParentView.findViewById(R.id.security_button);
        mSecurityIconResource = 0;

        mVerboseStatusTextView = mParentView.findViewById(R.id.location_bar_verbose_status);

        mLocationBarButtonType = LocationBarButtonType.NONE;
        mNavigationButton.setVisibility(View.INVISIBLE);
        mSecurityButton.setVisibility(View.INVISIBLE);

        AnimatorListenerAdapter iconChangeAnimatorListener = new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (animation == mSecurityButtonShowAnimator) {
                    mNavigationButton.setVisibility(View.INVISIBLE);
                } else if (animation == mNavigationIconShowAnimator) {
                    mSecurityButton.setVisibility(View.INVISIBLE);
                }
            }

            @Override
            public void onAnimationStart(Animator animation) {
                if (animation == mSecurityButtonShowAnimator) {
                    mSecurityButton.setVisibility(View.VISIBLE);
                } else if (animation == mNavigationIconShowAnimator) {
                    mNavigationButton.setVisibility(View.VISIBLE);
                }
            }
        };

        mSecurityButtonShowAnimator = new AnimatorSet();
        mSecurityButtonShowAnimator.playTogether(
                ObjectAnimator.ofFloat(mNavigationButton, View.ALPHA, 0),
                ObjectAnimator.ofFloat(mSecurityButton, View.ALPHA, 1));
        mSecurityButtonShowAnimator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        mSecurityButtonShowAnimator.addListener(iconChangeAnimatorListener);

        mNavigationIconShowAnimator = new AnimatorSet();
        mNavigationIconShowAnimator.playTogether(
                ObjectAnimator.ofFloat(mNavigationButton, View.ALPHA, 1),
                ObjectAnimator.ofFloat(mSecurityButton, View.ALPHA, 0));
        mNavigationIconShowAnimator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        mNavigationIconShowAnimator.addListener(iconChangeAnimatorListener);

        Resources res = mParentView.getResources();
        mUrlMinWidth = res.getDimensionPixelSize(R.dimen.location_bar_min_url_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_start_icon_width)
                + (res.getDimensionPixelSize(R.dimen.location_bar_lateral_padding) * 2);
        mStatusSeparatorWidth =
                res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_status_separator_spacer);
        mVerboseStatusMinWidth = mStatusSeparatorWidth
                + res.getDimensionPixelSize(R.dimen.location_bar_min_verbose_status_text_width);
    }

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mToolbarDataProvider = toolbarDataProvider;
    }

    /**
     * Provides the WindowAndroid for the parent component. Used to retreive the current activity
     * on demand.
     * @param windowAndroid The current {@link WindowAndroid}.
     */
    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mSecurityButton.setOnClickListener(this);
        mNavigationButton.setOnClickListener(this);
        mVerboseStatusTextView.setOnClickListener(this);
    }

    /**
     * @param urlHasFocus Whether the url currently has focus.
     */
    public void onUrlFocusChange(boolean urlHasFocus) {
        mUrlHasFocus = urlHasFocus;
        changeLocationBarIcon();
        updateVerboseStatusVisibility();
        updateLocationBarIconContainerVisibility();
    }

    /**
     * @param useDarkColors Whether dark colors should be for the status icon and text.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        mUseDarkColors = useDarkColors;
        updateSecurityIcon();
        setNavigationButtonType(mNavigationButtonType);
    }

    @LocationBarButtonType
    private int getLocationBarButtonToShow() {
        // The navigation icon type is only applicable on tablets.  While smaller form factors do
        // not have an icon visible to the user when the URL is focused, BUTTON_TYPE_NONE is not
        // returned as it will trigger an undesired jump during the animation as it attempts to
        // hide the icon.
        if (mUrlHasFocus && mIsTablet) return LocationBarButtonType.NAVIGATION_ICON;

        return mToolbarDataProvider.getSecurityIconResource(mIsTablet) != 0
                ? LocationBarButtonType.SECURITY_ICON
                : LocationBarButtonType.NONE;
    }

    private void changeLocationBarIcon() {
        if (mLocationBarIconActiveAnimator != null && mLocationBarIconActiveAnimator.isRunning()) {
            mLocationBarIconActiveAnimator.cancel();
        }

        mLocationBarButtonType = getLocationBarButtonToShow();

        View viewToBeShown = null;
        switch (mLocationBarButtonType) {
            case LocationBarButtonType.SECURITY_ICON:
                viewToBeShown = mSecurityButton;
                mLocationBarIconActiveAnimator = mSecurityButtonShowAnimator;
                break;
            case LocationBarButtonType.NAVIGATION_ICON:
                viewToBeShown = mNavigationButton;
                mLocationBarIconActiveAnimator = mNavigationIconShowAnimator;
                break;
            case LocationBarButtonType.NONE:
            default:
                mLocationBarIconActiveAnimator = null;
                return;
        }

        if (viewToBeShown.getVisibility() == View.VISIBLE && viewToBeShown.getAlpha() == 1) {
            return;
        }
        if (mDelegate.shouldAnimateIconChanges()) {
            mLocationBarIconActiveAnimator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        } else {
            mLocationBarIconActiveAnimator.setDuration(0);
        }
        mLocationBarIconActiveAnimator.start();
    }

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    public void updateSecurityIcon() {
        @DrawableRes
        int id = mToolbarDataProvider.getSecurityIconResource(mIsTablet);
        if (id == 0) {
            mSecurityButton.setImageDrawable(null);
        } else {
            // ImageView#setImageResource is no-op if given resource is the current one.
            mSecurityButton.setImageResource(id);
            ApiCompatibilityUtils.setImageTintList(
                    mSecurityButton, mToolbarDataProvider.getSecurityIconColorStateList());
        }

        int contentDescriptionId = mToolbarDataProvider.getSecurityIconContentDescription();
        String contentDescription = mParentView.getContext().getString(contentDescriptionId);
        mSecurityButton.setContentDescription(contentDescription);

        updateVerboseStatusVisibility();

        if (mSecurityIconResource == id && mLocationBarButtonType == getLocationBarButtonToShow()) {
            return;
        }
        mSecurityIconResource = id;

        changeLocationBarIcon();
        updateLocationBarIconContainerVisibility();
    }

    /**
     * @return The view displaying the security icon.
     */
    public View getSecurityIconView() {
        return mSecurityButton;
    }

    /**
     * @return Whether the security button is currently being displayed.
     */
    @VisibleForTesting
    public boolean isSecurityButtonShown() {
        return mLocationBarButtonType == LocationBarButtonType.SECURITY_ICON;
    }

    /**
     * @return The ID of the drawable currently shown in the security icon.
     */
    @VisibleForTesting
    @DrawableRes
    public int getSecurityIconResourceId() {
        return mSecurityIconResource;
    }

    /**
     * Sets the type of the current navigation type and updates the UI to match it.
     * @param buttonType The type of navigation button to be shown.
     */
    public void setNavigationButtonType(@NavigationButtonType int buttonType) {
        // TODO(twellington): Return early if the navigation button type and tint hasn't changed.
        if (!mIsTablet) return;
        switch (buttonType) {
            case NavigationButtonType.PAGE:
                Drawable page = TintedDrawable.constructTintedDrawable(mParentView.getContext(),
                        R.drawable.ic_omnibox_page,
                        mUseDarkColors ? R.color.dark_mode_tint : R.color.light_mode_tint);
                mNavigationButton.setImageDrawable(page);
                break;
            case NavigationButtonType.MAGNIFIER:
                Drawable search = TintedDrawable.constructTintedDrawable(mParentView.getContext(),
                        R.drawable.omnibox_search,
                        mUseDarkColors ? R.color.dark_mode_tint : R.color.light_mode_tint);
                mNavigationButton.setImageDrawable(search);
                break;
            case NavigationButtonType.EMPTY:
                mNavigationButton.setImageDrawable(null);
                break;
            default:
                assert false;
        }

        if (mNavigationButton.getVisibility() != View.VISIBLE) {
            mNavigationButton.setVisibility(View.VISIBLE);
        }
        mNavigationButtonType = buttonType;

        updateLocationBarIconContainerVisibility();
    }

    /**
     * @param visible Whether the navigation button should be visible.
     */
    public void setSecurityButtonVisibility(boolean visible) {
        if (!visible && mSecurityButton.getVisibility() == View.VISIBLE) {
            mSecurityButton.setVisibility(View.GONE);
        } else if (visible && mSecurityButton.getVisibility() == View.GONE
                && mSecurityButton.getDrawable() != null
                && mSecurityButton.getDrawable().getIntrinsicWidth() > 0
                && mSecurityButton.getDrawable().getIntrinsicHeight() > 0) {
            mSecurityButton.setVisibility(View.VISIBLE);
        }
    }

    /**
     * Update visibility of the verbose status based on the button type and focus state of the
     * omnibox.
     */
    private void updateVerboseStatusVisibility() {
        boolean verboseStatusVisible = !mUrlHasFocus
                && mToolbarDataProvider.shouldShowVerboseStatus() && mHasSpaceForVerboseStatus;

        int verboseStatusVisibility = verboseStatusVisible ? View.VISIBLE : View.GONE;

        mVerboseStatusTextView.setVisibility(verboseStatusVisibility);

        View separator = mParentView.findViewById(R.id.location_bar_verbose_status_separator);
        separator.setVisibility(verboseStatusVisibility);

        mParentView.findViewById(R.id.location_bar_verbose_status_extra_space)
                .setVisibility(verboseStatusVisibility);

        if (!verboseStatusVisible) {
            // Return early since everything past here requires the verbose status to be visible
            // and able to be populated with content.
            return;
        }

        mVerboseStatusTextView.setText(mToolbarDataProvider.getVerboseStatusString());
        mVerboseStatusTextView.setTextColor(mToolbarDataProvider.getVerboseStatusTextColor(
                mParentView.getResources(), mUseDarkColors));

        separator.setBackgroundColor(mToolbarDataProvider.getVerboseStatusSeparatorColor(
                mParentView.getResources(), mUseDarkColors));
    }

    /**
     * Update the visibility of the location bar icon container based on the state of the
     * security and navigation icons.
     */
    protected void updateLocationBarIconContainerVisibility() {
        @LocationBarButtonType
        int buttonToShow = getLocationBarButtonToShow();
        mParentView.findViewById(R.id.location_bar_icon)
                .setVisibility(
                        buttonToShow != LocationBarButtonType.NONE ? View.VISIBLE : View.GONE);
    }

    /**
     * Whether {@code v} is a view (location icon, verbose status, ...) which can be clicked to
     * show the Page Info popup.
     */
    public boolean shouldShowPageInfoForView(View v) {
        return v == mSecurityButton || v == mNavigationButton || v == mVerboseStatusTextView;
    }

    @Override
    public void onClick(View view) {
        if (mUrlHasFocus || !shouldShowPageInfoForView(view)) return;

        if (!mToolbarDataProvider.hasTab() || mToolbarDataProvider.getTab().getWebContents() == null
                || mWindowAndroid == null) {
            return;
        }

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            PageInfoController.show(activity, mToolbarDataProvider.getTab(), null,
                    PageInfoController.OpenedFromSource.TOOLBAR);
        }
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     * This value is used to determine whether the verbose status text should be visible.
     * @param unfocusedWidth The unfocused location bar width.
     */
    public void setUnfocusedLocationBarWidth(float unfocusedWidth) {
        if (mUnfocusedLocationBarWidth == unfocusedWidth) return;

        // This unfocused with is used rather than observing #onMeasure() to avoid showing the
        // verbose status when the animation to unfocus the URL bar has finished. There is a call to
        // LocationBarLayout#onMeasure() after the URL focus animation has finished and before the
        // location bar has received its updated width layout param.
        mUnfocusedLocationBarWidth = unfocusedWidth;

        boolean previousHasSpace = mHasSpaceForVerboseStatus;
        mHasSpaceForVerboseStatus =
                mUnfocusedLocationBarWidth >= mUrlMinWidth + mVerboseStatusMinWidth;

        if (mHasSpaceForVerboseStatus) {
            int previousMaxWidth = mVerboseStatusTextMaxWidth;
            mVerboseStatusTextMaxWidth =
                    (int) (mUnfocusedLocationBarWidth - mUrlMinWidth - mStatusSeparatorWidth);

            // Skip setting the max width if it hasn't changed since TextView#setMaxWidth
            // invalidates the view and requests a layout.
            if (previousMaxWidth != mVerboseStatusTextMaxWidth) {
                mVerboseStatusTextView.setMaxWidth(mVerboseStatusTextMaxWidth);
            }
        }

        if (previousHasSpace != mHasSpaceForVerboseStatus) updateVerboseStatusVisibility();
    }
}
