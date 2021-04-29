// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RotateDrawable;
import android.graphics.drawable.TransitionDrawable;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * StatusView is a location bar's view displaying status (icons and/or text).
 */
public class StatusView extends LinearLayout {
    @IntDef({IconTransitionType.CROSSFADE, IconTransitionType.ROTATE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconTransitionType {
        int CROSSFADE = 0;
        int ROTATE = 1;
    }
    public static final int ICON_ANIMATION_DURATION_MS = 225;
    public static final int ICON_ROTATION_DURATION_MS = 250;
    private static final int ICON_ROTATION_DEGREES = 180;

    private @Nullable View mIncognitoBadge;
    private int mIncognitoBadgeEndPaddingWithIcon;
    private int mIncognitoBadgeEndPaddingWithoutIcon;
    private int mTouchDelegateStartOffset;
    private int mTouchDelegateEndOffset;

    private ImageView mIconView;
    private TextView mVerboseStatusTextView;
    private View mSeparatorView;
    private View mStatusExtraSpace;

    private boolean mAnimationsEnabled;
    private boolean mAnimatingStatusIconShow;
    private boolean mAnimatingStatusIconHide;

    private @StringRes int mAccessibilityToast;

    private Drawable mStatusIconDrawable;

    private TouchDelegate mTouchDelegate;
    private CompositeTouchDelegate mCompositeTouchDelegate;

    private boolean mLastTouchDelegateRtlness;
    private Rect mLastTouchDelegateRect;

    private LocationBarDataProvider mLocationBarDataProvider;
    private SearchEngineLogoUtils mSearchEngineLogoUtils;

    public StatusView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconView = findViewById(R.id.location_bar_status_icon);
        mVerboseStatusTextView = findViewById(R.id.location_bar_verbose_status);
        mSeparatorView = findViewById(R.id.location_bar_verbose_status_separator);
        mStatusExtraSpace = findViewById(R.id.location_bar_verbose_status_extra_space);

        configureAccessibilityDescriptions();
    }

    void setLocationBarDataProvider(LocationBarDataProvider toolbarCommonPropertiesModel) {
        mLocationBarDataProvider = toolbarCommonPropertiesModel;
    }

    void setSearchEngineLogoUtils(SearchEngineLogoUtils searchEngineLogoUtils) {
        mSearchEngineLogoUtils = searchEngineLogoUtils;
    }

    /**
     * Update information required to display the search engine icon.
     */
    public void updateSearchEngineStatusIcon() {
        if (!mSearchEngineLogoUtils.isSearchEngineLogoEnabled()) return;

        LinearLayout.LayoutParams layoutParams =
                new LinearLayout.LayoutParams(mIconView.getLayoutParams());
        layoutParams.setMarginEnd(0);
        layoutParams.width =
                getResources().getDimensionPixelSize(R.dimen.location_bar_status_icon_width);
        mIconView.setLayoutParams(layoutParams);
        // Setup the padding once we're loaded, the other padding changes will happen with post-
        // layout positioning.
        setPaddingRelative(getPaddingStart(), getPaddingTop(),
                getResources().getDimensionPixelOffset(R.dimen.sei_location_bar_icon_end_padding),
                getPaddingBottom());
        // Note: the margins and implicit padding were removed from the status view for the
        // dse icon experiment. Moving padding values that were there to the verbose status
        // text view and the verbose text extra space.
        mVerboseStatusTextView.setPaddingRelative(
                getResources().getDimensionPixelSize(
                        R.dimen.sei_location_bar_verbose_start_padding_verbose_text),
                mVerboseStatusTextView.getPaddingTop(), mVerboseStatusTextView.getPaddingEnd(),
                mVerboseStatusTextView.getPaddingBottom());
        layoutParams = new LinearLayout.LayoutParams(mStatusExtraSpace.getLayoutParams());
        layoutParams.width = getResources().getDimensionPixelSize(
                R.dimen.sei_location_bar_status_extra_padding_width);
        mStatusExtraSpace.setLayoutParams(layoutParams);
    }

    /**
     * @return Whether search engine status icon is visible.
     */
    public boolean isSearchEngineStatusIconVisible() {
        return mIconView.getVisibility() == VISIBLE;
    }

    /**
     * Set the composite touch delegate here to which this view's touch delegate will be added.
     * @param compositeTouchDelegate The parent's CompositeTouchDelegate to be used.
     */
    public void setCompositeTouchDelegate(CompositeTouchDelegate compositeTouchDelegate) {
        mCompositeTouchDelegate = compositeTouchDelegate;
        mIconView.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight,
                                                    oldBottom) -> updateTouchDelegate());
    }

    private void setupAndRunStatusIconAnimations(boolean wantIconHidden, boolean isIconHidden) {
        // This is to prevent the visibility of the view being changed both implicitly here and
        // explicitly in setStatusIconShown. The visibility should only be set here through code not
        // related to the dse icon.
        // Since the tablet never adjusts the alpha manually we want the implicit behavior.
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                && mLocationBarDataProvider != null
                && mSearchEngineLogoUtils.shouldShowSearchEngineLogo(
                        mLocationBarDataProvider.isIncognito())) {
            return;
        }

        if (!wantIconHidden && (isIconHidden || mAnimatingStatusIconHide)) {
            // Action 1: animate showing, if icon was either hidden or hiding.
            if (mAnimatingStatusIconHide) mIconView.animate().cancel();
            mAnimatingStatusIconHide = false;

            mAnimatingStatusIconShow = true;
            mIconView.setVisibility(View.VISIBLE);
            updateIncognitoBadgeEndPadding();
            mIconView.animate()
                    .alpha(1.0f)
                    .setDuration(ICON_ANIMATION_DURATION_MS)
                    .withEndAction(() -> {
                        mAnimatingStatusIconShow = false;
                        // Wait until the icon is visible so the bounds will be properly set.
                        updateTouchDelegate();
                    })
                    .start();
        } else if (wantIconHidden && (!isIconHidden || mAnimatingStatusIconShow)) {
            // Action 2: animate hiding, if icon was either shown or showing.
            if (mAnimatingStatusIconShow) mIconView.animate().cancel();
            mAnimatingStatusIconShow = false;

            mAnimatingStatusIconHide = true;
            // Do not animate phase-out when animations are disabled.
            // While this looks nice in some cases (navigating to insecure sites),
            // it has a side-effect of briefly showing padlock (phase-out) when navigating
            // back and forth between secure and insecure sites, which seems like a glitch.
            // See bug: crbug.com/919449
            mIconView.animate()
                    .setDuration(mAnimationsEnabled ? ICON_ANIMATION_DURATION_MS : 0)
                    .alpha(0.0f)
                    .withEndAction(() -> {
                        mIconView.setVisibility(View.GONE);
                        mAnimatingStatusIconHide = false;
                        // Update incognito badge padding after the animation to avoid a glitch on
                        // focusing location bar.
                        updateIncognitoBadgeEndPadding();
                        updateTouchDelegate();
                    })
                    .start();
        } else {
            updateTouchDelegate();
        }
    }

    /**
     * Start animating transition of status icon.
     * @param transitionType The animation transition type for the icon.
     * @param animationFinishedCallback The callback to be run after the status icon has been
     *                                  successfully set.
     */
    private void animateStatusIcon(
            @IconTransitionType int transitionType, @Nullable Runnable animationFinishedCallback) {
        Drawable targetIcon = mStatusIconDrawable;
        boolean wantIconHidden = mStatusIconDrawable == null;
        // We only handle additional callback after rotation.
        assert transitionType == IconTransitionType.ROTATE || animationFinishedCallback == null;

        // Ensure proper handling of animations.
        // Possible variants:
        // 1. Is: shown,           want: hidden  => animate hiding,
        // 2. Is: shown,           want: shown   => crossfade w/TransitionDrawable,
        // 3. Is: animating(show), want: hidden  => cancel animation; animate hiding,
        // 4. Is: animating(show), want: shown   => crossfade (carry on showing),
        // 5. Is: animating(hide), want: hidden  => no op,
        // 6. Is: animating(hide), want: shown   => cancel animation; animate showing; crossfade,
        // 7. Is: hidden,          want: hidden  => no op,
        // 8. Is: hidden,          want: shown   => animate showing.
        //
        // This gives 3 actions:
        // - Animate showing, if hidden or previously hiding (6 + 8); cancel previous animation (6)
        // - Animate hiding, if shown or previously showing (1 + 3); cancel previous animation (3)
        // - crossfade w/TransitionDrawable, if visible (2, 4, 6), otherwise use image directly.
        // All other options (5, 7) are no-op.
        //
        // Note: this will be compacted once we start using LayoutTransition with StatusView.

        boolean isIconHidden = mIconView.getVisibility() == View.GONE;

        // Actions 1 and 2 occur in #setupAndRunStatusIconAnimations.
        // TODO(crbug.com/1019488): Consolidate animation behavior once the dse icon feature ships.
        setupAndRunStatusIconAnimations(wantIconHidden, isIconHidden);

        // Action 3: Specify icon content. Use TransitionDrawable whenever object is visible.
        if (targetIcon != null) {
            if (!isIconHidden) {
                Drawable existingDrawable = mIconView.getDrawable();
                if (existingDrawable instanceof TransitionDrawable
                        && ((TransitionDrawable) existingDrawable).getNumberOfLayers() == 2) {
                    existingDrawable = ((TransitionDrawable) existingDrawable).getDrawable(1);
                }

                TransitionDrawable newImage = new TransitionDrawable(new Drawable[] {
                        existingDrawable,
                        transitionType == IconTransitionType.ROTATE ? getRotatedIcon(targetIcon)
                                                                    : targetIcon});

                mIconView.setImageDrawable(newImage);

                // Note: crossfade controls blending, not animation.
                newImage.setCrossFadeEnabled(true);

                if (transitionType == IconTransitionType.CROSSFADE) {
                    newImage.startTransition(mAnimationsEnabled ? ICON_ANIMATION_DURATION_MS : 0);
                } else {
                    mIconView.animate()
                            .setDuration(ICON_ROTATION_DURATION_MS)
                            .rotationBy(ICON_ROTATION_DEGREES)
                            .setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR)
                            .withStartAction(
                                    () -> { newImage.startTransition(ICON_ANIMATION_DURATION_MS); })
                            .withEndAction(() -> {
                                mIconView.setRotation(0);
                                // Only update status icon if it is still the current icon.
                                if (mStatusIconDrawable == targetIcon) {
                                    mIconView.setImageDrawable(targetIcon);
                                    if (animationFinishedCallback != null) {
                                        animationFinishedCallback.run();
                                    }
                                }
                            })
                            .start();
                }

                // Update the touch delegate only if the icons are swapped without animating the
                // image view.
                if (!mAnimatingStatusIconShow) updateTouchDelegate();
            } else {
                mIconView.setImageDrawable(targetIcon);
            }
        }
    }

    /** Returns a rotated version of the icon passed in. */
    private Drawable getRotatedIcon(@NonNull Drawable icon) {
        RotateDrawable rotated = new RotateDrawable();
        rotated.setDrawable(icon);
        rotated.setToDegrees(ICON_ROTATION_DEGREES);
        // Jump drawable to its target state.
        rotated.setLevel(10000);
        return rotated;
    }

    /**
     * Specify object to receive click events.
     *
     * @param listener Instance of View.OnClickListener or null.
     */
    void setStatusClickListener(View.OnClickListener listener) {
        mIconView.setOnClickListener(listener);
        mVerboseStatusTextView.setOnClickListener(listener);
    }

    /**
     * Configure accessibility toasts.
     */
    void configureAccessibilityDescriptions() {
        View.OnLongClickListener listener = new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                if (mAccessibilityToast == 0) return false;
                Context context = getContext();
                return Toast.showAnchoredToast(
                        context, view, context.getResources().getString(mAccessibilityToast));
            }
        };
        mIconView.setOnLongClickListener(listener);
    }

    /**
     * Toggle use of animations.
     */
    void setAnimationsEnabled(boolean enabled) {
        mAnimationsEnabled = enabled;
    }

    /**
     * Sets the Drawable to be used as the status icon.
     * @param statusIconDrawable The Drawable.
     * @param transitionType The animation transition type for the icon.
     * @param animationFinishedCallback The callback to be run after the new drawable has been
     *                                  successfully set.
     */
    void setStatusIconResources(@Nullable Drawable statusIconDrawable,
            @IconTransitionType int transitionType, @Nullable Runnable animationFinishedCallback) {
        mStatusIconDrawable = statusIconDrawable;
        animateStatusIcon(transitionType, animationFinishedCallback);
    }

    /** Specify the status icon alpha. */
    void setStatusIconAlpha(float alpha) {
        if (mIconView == null) return;
        mIconView.setAlpha(alpha);
    }

    /** Specify the status icon visibility. */
    void setStatusIconShown(boolean showIcon) {
        if (mIconView == null) return;

        // This is to prevent the visibility of the view being changed both explicitly here and
        // implicitly in animateStatusIcon. The visibility should only be set here through code
        // related to the dse icon.
        if (mLocationBarDataProvider != null
                && !mSearchEngineLogoUtils.shouldShowSearchEngineLogo(
                        mLocationBarDataProvider.isIncognito())) {
            // Let developers know that they shouldn't use this code-path.
            assert false : "Only DSE icon code should set the status icon visibility manually.";
            return;
        }

        mIconView.setVisibility(showIcon ? VISIBLE : GONE);
        updateTouchDelegate();
    }

    /**
     * Specify accessibility string presented to user upon long click.
     */
    void setStatusIconAccessibilityToast(@StringRes int description) {
        mAccessibilityToast = description;
    }

    /**
     * Specify content description for security icon.
     */
    void setStatusIconDescription(@StringRes int descriptionRes) {
        String description = null;
        int importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO;
        if (descriptionRes != 0) {
            description = getResources().getString(descriptionRes);
            importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES;
        }
        mIconView.setContentDescription(description);
        mIconView.setImportantForAccessibility(importantForAccessibility);
    }

    /**
     * Select color of Separator view.
     */
    void setSeparatorColor(@ColorRes int separatorColor) {
        mSeparatorView.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), separatorColor));
    }

    /**
     * Select color of verbose status text.
     */
    void setVerboseStatusTextColor(@ColorRes int textColor) {
        mVerboseStatusTextView.setTextColor(
                ApiCompatibilityUtils.getColor(getResources(), textColor));
    }

    /**
     * Specify content of the verbose status text.
     */
    void setVerboseStatusTextContent(@StringRes int content) {
        mVerboseStatusTextView.setText(content);
    }

    /**
     * Specify visibility of the verbose status text.
     */
    void setVerboseStatusTextVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        mVerboseStatusTextView.setVisibility(visibility);
        mSeparatorView.setVisibility(visibility);
        mStatusExtraSpace.setVisibility(visibility);
    }

    /**
     * Specify width of the verbose status text.
     */
    void setVerboseStatusTextWidth(int width) {
        mVerboseStatusTextView.setMaxWidth(width);
    }

    /**
     * @param incognitoBadgeVisible Whether or not the incognito badge is visible.
     */
    void setIncognitoBadgeVisibility(boolean incognitoBadgeVisible) {
        // Initialize the incognito badge on the first time it becomes visible.
        if (mIncognitoBadge == null && !incognitoBadgeVisible) return;
        if (mIncognitoBadge == null) initializeIncognitoBadge();

        mIncognitoBadge.setVisibility(incognitoBadgeVisible ? View.VISIBLE : View.GONE);
        updateTouchDelegate();
    }

    private void initializeIncognitoBadge() {
        ViewStub viewStub = findViewById(R.id.location_bar_incognito_badge_stub);
        mIncognitoBadge = viewStub.inflate();
        mIncognitoBadgeEndPaddingWithIcon = getResources().getDimensionPixelSize(
                R.dimen.location_bar_incognito_badge_end_padding_with_status_icon);
        mIncognitoBadgeEndPaddingWithoutIcon = getResources().getDimensionPixelSize(
                R.dimen.location_bar_incognito_badge_end_padding_without_status_icon);
        updateIncognitoBadgeEndPadding();
    }

    private void updateIncognitoBadgeEndPadding() {
        if (mIncognitoBadge == null) return;

        int endPadding = -1;
        if (mSearchEngineLogoUtils.isSearchEngineLogoEnabled()) {
            endPadding = 0;
        } else {
            endPadding = isIconVisible() ? mIncognitoBadgeEndPaddingWithIcon
                                         : mIncognitoBadgeEndPaddingWithoutIcon;
        }
        mIncognitoBadge.setPaddingRelative(mIncognitoBadge.getPaddingStart(),
                mIncognitoBadge.getPaddingTop(), endPadding, mIncognitoBadge.getPaddingBottom());
    }

    /**
     * Create a touch delegate to expand the clickable area for the padlock icon (see
     * crbug.com/970031 for motivation/info). This method will be called when the icon is animating
     * in and when layout changes. It's called on these intervals because (1) the layout could
     * change and (2) the Rtl-ness of the view could change. There are checks in place to avoid
     * doing unnecessary work, so if the rect is empty or equivalent to the one already in place,
     * no work will be done.
     */
    private void updateTouchDelegate() {
        if (!isIconVisible()) {
            // Tear down the existing delegate if it exists.
            if (mTouchDelegate != null) {
                mCompositeTouchDelegate.removeDelegateForDescendantView(mTouchDelegate);
                mTouchDelegate = null;
                mLastTouchDelegateRect = new Rect();
            }
            return;
        }
        // Setup a touch delegate to increase the clickable area for the padlock.
        // See for more information.
        Rect touchDelegateBounds = new Rect();
        mIconView.getHitRect(touchDelegateBounds);
        if (touchDelegateBounds.equals(new Rect())) return;

        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if (mTouchDelegateStartOffset == 0) {
            mTouchDelegateStartOffset =
                    getResources().getDimensionPixelSize(R.dimen.location_bar_lateral_padding);
        }
        if (mTouchDelegateEndOffset == 0) {
            mTouchDelegateEndOffset =
                    getResources().getDimensionPixelSize(R.dimen.location_bar_icon_margin_end);
        }
        touchDelegateBounds.left -= isRtl ? mTouchDelegateEndOffset : mTouchDelegateStartOffset;
        touchDelegateBounds.right += isRtl ? mTouchDelegateStartOffset : mTouchDelegateEndOffset;

        // If our rect and rtl-ness hasn't changed, there's no need to recreate the TouchDelegate.
        if (mTouchDelegate != null && touchDelegateBounds.equals(mLastTouchDelegateRect)
                && mLastTouchDelegateRtlness == isRtl) {
            return;
        }
        mLastTouchDelegateRect = touchDelegateBounds;

        // Remove the existing delegate when we recreate a new one.
        if (mTouchDelegate != null) {
            mCompositeTouchDelegate.removeDelegateForDescendantView(mTouchDelegate);
        }

        // Set the delegate on LocationBarLayout because it has available space. Setting on
        // status view itself will clip the rect.
        mTouchDelegate = new TouchDelegate(touchDelegateBounds, mIconView);
        mCompositeTouchDelegate.addDelegateForDescendantView(mTouchDelegate);
        mLastTouchDelegateRtlness = isRtl;
    }

    // TODO(ender): The final last purpose of this method is to allow
    // ToolbarButtonInProductHelpController set up help bubbles. This dependency is about to
    // change. Do not depend on this method when creating new code.
    View getSecurityButton() {
        return mIconView;
    }

    /**
     * @return The width of the status icon including start/end margins.
     */
    int getStatusIconWidth() {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) getLayoutParams();
        return lp.getMarginStart() + getMeasuredWidth() + lp.getMarginEnd();
    }

    /** @return True if the status icon is currently visible. */
    private boolean isIconVisible() {
        return mStatusIconDrawable != null && mIconView.getVisibility() != GONE
                && mIconView.getAlpha() != 0;
    }

    TouchDelegate getTouchDelegateForTesting() {
        return mTouchDelegate;
    }
}
