// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.util.List;

/**
 * A location bar implementation specific for smaller/phone screens.
 */
public class LocationBarPhone extends LocationBarLayout {
    private static final int ACTION_BUTTON_TOUCH_OVERFLOW_LEFT = 15;

    private View mFirstVisibleFocusedView;
    private View mUrlBar;
    private View mStatusView;

    /**
     * Constructor used to inflate from XML.
     */
    public LocationBarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mUrlBar = findViewById(R.id.url_bar);
        mStatusView = findViewById(R.id.location_bar_status);
        // Assign the first visible view here only if it hasn't been set by the DSE icon experiment.
        // See onNativeLibrary ready for when this variable is set for the DSE icon case.
        mFirstVisibleFocusedView =
                mFirstVisibleFocusedView == null ? mUrlBar : mFirstVisibleFocusedView;

        Rect delegateArea = new Rect();
        mUrlActionContainer.getHitRect(delegateArea);
        delegateArea.left -= ACTION_BUTTON_TOUCH_OVERFLOW_LEFT;
        TouchDelegate touchDelegate = new TouchDelegate(delegateArea, mUrlActionContainer);
        assert mUrlActionContainer.getParent() == this;
        mCompositeTouchDelegate.addDelegateForDescendantView(touchDelegate);
    }

    @Override
    public void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl) {
        super.updateSearchEngineStatusIcon(
                shouldShowSearchEngineLogo, isSearchEngineGoogle, searchEngineUrl);

        // The search engine icon will be the first visible focused view when it's showing.
        shouldShowSearchEngineLogo = SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                getToolbarDataProvider().isIncognito());

        // This branch will be hit if the search engine logo experiment is enabled.
        if (SearchEngineLogoUtils.isSearchEngineLogoEnabled()) {
            // Setup the padding once we're loaded, the focused padding changes will happen with
            // post-layout positioning via setTranslation. This is a byproduct of the way we do the
            // omnibox un/focus animation which is by writing a function f(x) where x ranges from
            // 0 (totally unfocused) to 1 (totally focused). Positioning the location bar and it's
            // children this way doesn't affect the views' bounds (including hit rect). But these
            // hit rects are preserved for the views that matter (the icon and the url actions
            // container).
            int lateralPadding = getResources().getDimensionPixelOffset(
                    R.dimen.sei_location_bar_lateral_padding);
            setPaddingRelative(lateralPadding, getPaddingTop(), lateralPadding, getPaddingBottom());
        }

        // This branch will be hit if the search engine logo experiment is enabled and we should
        // show the logo.
        if (shouldShowSearchEngineLogo) {
            // When the search engine icon is enabled, icons are translations into the parent view's
            // padding area. Set clip padding to false to prevent them from getting clipped.
            setClipToPadding(false);
        }
        setShowIconsWhenUrlFocused(shouldShowSearchEngineLogo);
    }

    /**
     * @return Width of child views before the first view that would be visible when location bar is
     *         focused. The first visible, focused view should be either url bar or status icon.
     */
    public int getOffsetOfFirstVisibleFocusedView() {
        int visibleWidth = 0;
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child == mFirstVisibleFocusedView) break;
            if (child.getVisibility() == GONE) continue;
            visibleWidth += child.getMeasuredWidth();
        }
        return visibleWidth;
    }

    /**
     * Populates fade animators of status icon for location bar focus change animation.
     * @param animators The target list to add animators to.
     * @param startDelayMs Start delay of fade animation in milliseconds.
     * @param durationMs Duration of fade animation in milliseconds.
     * @param targetAlpha Target alpha value.
     */
    public void populateFadeAnimations(
            List<Animator> animators, long startDelayMs, long durationMs, float targetAlpha) {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child == mFirstVisibleFocusedView) break;
            Animator animator = ObjectAnimator.ofFloat(child, ALPHA, targetAlpha);
            animator.setStartDelay(startDelayMs);
            animator.setDuration(durationMs);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }
    }

    /**
     * Calculates the offset required for the focused LocationBar to appear as it's still unfocused
     * so it can animate to a focused state.
     *
     * @param hasFocus True if the LocationBar has focus, this will be true between the focus
     *                 animation starting and the unfocus animation starting.
     * @return The offset for the location bar when showing the dse icon.
     */
    public int getLocationBarOffsetForFocusAnimation(boolean hasFocus) {
        if (mStatusCoordinator == null) return 0;

        // No offset is required if the experiment is disabled.
        if (!SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                    getToolbarDataProvider().isIncognito())) {
            return 0;
        }

        // On non-NTP pages, there will always be an icon when unfocused.
        if (mToolbarDataProvider.getNewTabPageForCurrentTab() == null) return 0;

        // This offset is only required when the focus animation is running.
        if (!hasFocus) return 0;

        // We're on the NTP with the fakebox showing.
        // The value returned changes based on if the layout is LTR OR RTL.
        // For LTR, the value is negative because we are making space on the left-hand side.
        // For RTL, the value is positive because we are pushing the icon further to the
        // right-hand side.
        int offset = mStatusCoordinator.getStatusIconWidth() - getAdditionalOffsetForNTP();
        return getLayoutDirection() == LAYOUT_DIRECTION_RTL ? offset : -offset;
    }

    /**
     * Function used to position the url bar inside the location bar during omnibox animation.
     *
     * @param urlExpansionPercent The current expansion percent, 1 is fully focused and 0 is
     *                            completely unfocused.
     *  @param hasFocus True if the LocationBar has focus, this will be true between the focus
     *                 animation starting and the unfocus animation starting.
     *  @return The X translation for the URL bar, used in the toolbar animation.
     */
    public float getUrlBarTranslationXForToolbarAnimation(
            float urlExpansionPercent, boolean hasFocus) {
        // This will be called before status view is ready.
        if (mStatusCoordinator == null) return 0;

        // No offset is required if the experiment is disabled.
        if (!SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                    getToolbarDataProvider().isIncognito())) {
            return 0;
        }

        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        // The calculation here is:  the difference in padding between the focused vs unfocused
        // states and also accounts for the translation that the status icon will do. In the end,
        // this translation will be the distance that the url bar needs to travel to arrive at the
        // desired padding when focused.
        float translation =
                urlExpansionPercent * mStatusCoordinator.getEndPaddingPixelSizeOnFocusDelta();

        if (!hasFocus && mStatusCoordinator.isSearchEngineStatusIconVisible()
                && SearchEngineLogoUtils.currentlyOnNTP(mToolbarDataProvider)) {
            // When:
            // 1. unfocusing the LocationBar on the NTP.
            // 2. scrolling the fakebox to the LocationBar on the NTP.
            // The status icon and the URL bar text overlap in the animation.
            //
            // This branch calculates the negative distance the URL bar needs to travel to
            // completely overlap the status icon and end up in a state that matches the fakebox.
            float overStatusIconTranslation = translation
                    - (1f - urlExpansionPercent)
                            * (mStatusCoordinator.getStatusIconWidth()
                                    - getAdditionalOffsetForNTP());
            // The value returned changes based on if the layout is LTR or RTL.
            // For LTR, the value is negative because the status icon is left of the url bar on the
            // x/y plane.
            // For RTL, the value is positive because the status icon is right of the url bar on the
            // x/y plane.
            return isRtl ? -overStatusIconTranslation : overStatusIconTranslation;
        }

        return isRtl ? -translation : translation;
    }

    /**
     * Updates percentage of current the URL focus change animation.
     * @param percent 1.0 is 100% focused, 0 is completely unfocused.
     */
    public void setUrlFocusChangePercent(float percent) {
        mUrlFocusChangePercent = percent;

        if (percent > 0f) {
            mUrlActionContainer.setVisibility(VISIBLE);
        } else if (percent == 0f && !isUrlFocusChangeInProgress()) {
            // If a URL focus change is in progress, then it will handle setting the visibility
            // correctly after it completes.  If done here, it would cause the URL to jump due
            // to a badly timed layout call.
            mUrlActionContainer.setVisibility(GONE);
        }

        updateButtonVisibility();
        mStatusCoordinator.setUrlFocusChangePercent(percent);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            // Remove the focus of this view once the URL field has taken focus as this view no
            // longer needs it.
            setFocusable(false);
            setFocusableInTouchMode(false);
        }
        setUrlFocusChangeInProgress(true);
        updateShouldAnimateIconChanges();
        super.onUrlFocusChange(hasFocus);
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        boolean needsCanvasRestore = false;
        if (child == mUrlBar && mUrlActionContainer.getVisibility() == VISIBLE) {
            canvas.save();

            // Clip the URL bar contents to ensure they do not draw under the URL actions during
            // focus animations.  Based on the RTL state of the location bar, the url actions
            // container can be on the left or right side, so clip accordingly.
            if (mUrlBar.getLeft() < mUrlActionContainer.getLeft()) {
                canvas.clipRect(0, 0, (int) mUrlActionContainer.getX(), getBottom());
            } else {
                canvas.clipRect(mUrlActionContainer.getX() + mUrlActionContainer.getWidth(), 0,
                        getWidth(), getBottom());
            }
            needsCanvasRestore = true;
        }
        boolean retVal = super.drawChild(canvas, child, drawingTime);
        if (needsCanvasRestore) {
            canvas.restore();
        }
        return retVal;
    }

    @Override
    public void finishUrlFocusChange(boolean hasFocus) {
        super.finishUrlFocusChange(hasFocus);
        if (!hasFocus) {
            mUrlActionContainer.setVisibility(GONE);
        }
        mStatusCoordinator.onUrlAnimationFinished(hasFocus);
    }

    @Override
    protected void updateButtonVisibility() {
        super.updateButtonVisibility();
        updateMicButtonVisibility();
    }

    @Override
    public void updateShouldAnimateIconChanges() {
        notifyShouldAnimateIconChanges(isUrlBarFocused() || isUrlFocusChangeInProgress());
    }

    @Override
    public void setShowIconsWhenUrlFocused(boolean showIcon) {
        super.setShowIconsWhenUrlFocused(showIcon);
        mFirstVisibleFocusedView = showIcon ? mStatusView : mUrlBar;
        mStatusCoordinator.setShowIconsWhenUrlFocused(showIcon);
    }

    private int getAdditionalOffsetForNTP() {
        return getResources().getDimensionPixelSize(R.dimen.sei_search_box_lateral_padding)
                - getResources().getDimensionPixelSize(R.dimen.sei_location_bar_lateral_padding);
    }

    @Override
    public void updateVisualsForState() {
        super.updateVisualsForState();
        boolean isIncognito = getToolbarDataProvider().isIncognito();
        setShowIconsWhenUrlFocused(SearchEngineLogoUtils.shouldShowSearchEngineLogo(isIncognito));
        updateStatusVisibility();
    }

    @Override
    public void onTabLoadingNTP(NewTabPage ntp) {
        super.onTabLoadingNTP(ntp);
        updateStatusVisibility();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarPhone.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarPhone.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mAutocompleteCoordinator.setOverviewModeBehavior(overviewModeBehavior);
    }

    /** Update the status visibility according to the current state held in LocationBar. */
    private void updateStatusVisibility() {
        boolean incognito = getToolbarDataProvider().isIncognito();
        if (!SearchEngineLogoUtils.shouldShowSearchEngineLogo(incognito)) {
            return;
        }

        if (SearchEngineLogoUtils.currentlyOnNTP(mToolbarDataProvider)) {
            mStatusCoordinator.setStatusIconShown(hasFocus());
        } else {
            mStatusCoordinator.setStatusIconShown(true);
        }
    }
}
