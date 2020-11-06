// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;

/** View of the StartSurfaceToolbar */
class StartSurfaceToolbarView extends RelativeLayout {
    private NewTabButton mNewTabButton;
    private View mIncognitoSwitch;
    private View mLogo;
    @Nullable
    private ImageButton mIdentityDiscButton;
    private int mPrimaryColor;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;
    private ViewPropertyAnimator mVisibilityAnimator;

    private Rect mLogoRect = new Rect();
    private Rect mViewRect = new Rect();

    private boolean mShouldShow;
    private boolean mInStartSurfaceMode;
    private boolean mIsShowing;

    public StartSurfaceToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mNewTabButton = findViewById(R.id.new_tab_button);
        mIncognitoSwitch = findViewById(R.id.incognito_switch);
        mLogo = findViewById(R.id.logo);
        mIdentityDiscButton = findViewById(R.id.identity_disc_button);
        updatePrimaryColorAndTint(false);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // TODO(https://crbug.com/1040526)

        super.onLayout(changed, l, t, r, b);

        if (mLogo.getVisibility() == View.GONE) return;

        mLogoRect.set(mLogo.getLeft(), mLogo.getTop(), mLogo.getRight(), mLogo.getBottom());
        for (int viewIndex = 0; viewIndex < getChildCount(); viewIndex++) {
            View view = getChildAt(viewIndex);
            if (view == mLogo || view.getVisibility() == View.GONE) continue;

            assert view.getVisibility() == View.VISIBLE;
            mViewRect.set(view.getLeft(), view.getTop(), view.getRight(), view.getBottom());
            if (Rect.intersects(mLogoRect, mViewRect)) {
                mLogo.setVisibility(View.GONE);
                break;
            }
        }
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabButton.setOnClickListener(listener);
    }

    /**
     * Sets the Logo visibility. Logo will not show if screen is not wide enough.
     * @param isVisible Whether the Logo should be visible.
     */
    void setLogoVisibility(boolean isVisible) {
        mLogo.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @param isVisible Whether the menu button is visible.
     */
    void setMenuButtonVisibility(boolean isVisible) {
        final int buttonPaddingLeft = getContext().getResources().getDimensionPixelOffset(
                R.dimen.start_surface_toolbar_button_padding_to_button);
        final int buttonPaddingRight =
                (isVisible ? buttonPaddingLeft
                           : getContext().getResources().getDimensionPixelOffset(
                                   R.dimen.start_surface_toolbar_button_padding_to_edge));
        mIdentityDiscButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
        mNewTabButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
    }

    /**
     * @param isVisible Whether the new tab button is visible.
     */
    void setNewTabButtonVisibility(boolean isVisible) {
        mNewTabButton.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        if (isVisible && Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            // This is a workaround for the issue that the UrlBar is given the default focus on
            // Android versions before Pie when showing the start surface toolbar with the new tab
            // button (UrlBar is invisible to users). Check crbug.com/1081538 for more details.
            mNewTabButton.getParent().requestChildFocus(mNewTabButton, mNewTabButton);
        }
    }

    /**
     * @param isClickable Whether the buttons are clickable.
     */
    void setButtonClickableState(boolean isClickable) {
        mNewTabButton.setClickable(isClickable);
        mIncognitoSwitch.setClickable(isClickable);
    }

    /**
     * @param isVisible Whether the Incognito switcher is visible.
     */
    void setIncognitoSwitcherVisibility(boolean isVisible) {
        mIncognitoSwitch.setVisibility(isVisible ? VISIBLE : GONE);
    }

    /**
     * @param isAtStart Whether the new tab button is at start.
     */
    void setNewTabButtonAtStart(boolean isAtStart) {
        assert isAtStart;
        if (isAtStart) {
            ((LayoutParams) mNewTabButton.getLayoutParams()).removeRule(RelativeLayout.START_OF);

            LayoutParams params = (LayoutParams) mIncognitoSwitch.getLayoutParams();
            params.removeRule(RelativeLayout.ALIGN_PARENT_START);
            params.addRule(RelativeLayout.CENTER_HORIZONTAL);
        }
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        if (mNewTabButton == null) return;
        if (highlight) {
            ViewHighlighter.turnOnCircularHighlight(mNewTabButton);
        } else {
            ViewHighlighter.turnOffHighlight(mNewTabButton);
        }
    }

    /** Called when incognito mode changes. */
    void updateIncognito(boolean isIncognito) {
        updatePrimaryColorAndTint(isIncognito);
    }

    /**
     * @param provider The {@link IncognitoStateProvider} passed to buttons that need access to it.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mNewTabButton.setIncognitoStateProvider(provider);
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mNewTabButton.onAccessibilityStatusChanged();
    }

    /** @return The View for the identity disc. */
    View getIdentityDiscView() {
        return mIdentityDiscButton;
    }

    /**
     * @param isAtStart Whether the identity disc is at start.
     */
    void setIdentityDiscAtStart(boolean isAtStart) {
        ((LayoutParams) mIdentityDiscButton.getLayoutParams()).removeRule(RelativeLayout.START_OF);
    }

    /**
     * @param isVisible Whether the identity disc is visible.
     */
    void setIdentityDiscVisibility(boolean isVisible) {
        mIdentityDiscButton.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the identity disc button is
     * pressed.
     * @param listener The callback that will be notified when the identity disc  is pressed.
     */
    void setIdentityDiscClickHandler(View.OnClickListener listener) {
        mIdentityDiscButton.setOnClickListener(listener);
    }

    /**
     * Updates the image displayed on the identity disc button.
     * @param image The new image for the button.
     */
    void setIdentityDiscImage(Drawable image) {
        mIdentityDiscButton.setImageDrawable(image);
    }

    /**
     * Updates idnetity disc content description.
     * @param contentDescriptionResId The new description for the button.
     */
    void setIdentityDiscContentDescription(@StringRes int contentDescriptionResId) {
        mIdentityDiscButton.setContentDescription(
                getContext().getResources().getString(contentDescriptionResId));
    }

    /**
     * Show or hide toolbar from tab.
     * @param inStartSurfaceMode Whether or not toolbar should be shown or hidden.
     * */
    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        mInStartSurfaceMode = inStartSurfaceMode;
        // When showing or hiding toolbar from a tab, the fade-in and fade-out animations are not
        // needed. (eg: cold start, changing theme, changing incognito status...)
        showStartSurfaceToolbar(mInStartSurfaceMode && mShouldShow, false);
    }

    /**
     * Show or hide toolbar.
     * @param shouldShowStartSurfaceToolbar Whether or not toolbar should be shown or hidden.
     * */
    void setToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mShouldShow = shouldShowStartSurfaceToolbar;
        // When simply setting visibility, the animations should be shown. (eg: search box has
        // focus)
        showStartSurfaceToolbar(mInStartSurfaceMode && mShouldShow, true);
    }

    /**
     * Start animation to show or hide toolbar.
     * @param showStartSurfaceToolbar Whether or not toolbar should be shown or hidden.
     * @param showAnimation Whether or not to show the animation.
     */
    private void showStartSurfaceToolbar(boolean showStartSurfaceToolbar, boolean showAnimation) {
        if (showStartSurfaceToolbar == mIsShowing) return;

        if (mVisibilityAnimator != null) {
            mVisibilityAnimator.cancel();
            finishAnimation(showStartSurfaceToolbar);
        }

        mIsShowing = showStartSurfaceToolbar;

        if (DeviceClassManager.enableAccessibilityLayout()) {
            finishAnimation(showStartSurfaceToolbar);
            return;
        }

        // TODO(https://crbug.com/1139024): Show the fade-in animation when
        // TabUiFeatureUtilities#isTabToGtsAnimationEnabled is true.
        if (!showAnimation) {
            setVisibility(showStartSurfaceToolbar ? View.VISIBLE : View.GONE);
            return;
        }

        // Show the fade-in and fade-out animation. Set visibility as VISIBLE here to show the
        // animation. The visibility will be finally set in finishAnimation().
        setVisibility(View.VISIBLE);
        setAlpha(showStartSurfaceToolbar ? 0.0f : 1.0f);

        final long duration = TopToolbarCoordinator.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS;

        mVisibilityAnimator =
                animate()
                        .alpha(showStartSurfaceToolbar ? 1.0f : 0.0f)
                        .setDuration(duration)
                        .setInterpolator(Interpolators.LINEAR_INTERPOLATOR)
                        .withEndAction(() -> { finishAnimation(showStartSurfaceToolbar); });
    }

    private void finishAnimation(boolean showStartSurfaceToolbar) {
        setAlpha(1.0f);
        setVisibility(showStartSurfaceToolbar ? View.VISIBLE : View.GONE);
        mVisibilityAnimator = null;
    }

    private void updatePrimaryColorAndTint(boolean isIncognito) {
        int primaryColor = ChromeColors.getPrimaryBackgroundColor(getResources(), isIncognito);
        setBackgroundColor(primaryColor);

        if (mLightIconTint == null) {
            mLightIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_light_tint_list);
            mDarkIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_tint_list);
        }
    }
}
