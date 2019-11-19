// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.view.WindowManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.status.StatusView;

/**
 * A location bar implementation specific for smaller/phone screens.
 */
public class LocationBarPhone extends LocationBarLayout {
    private static final int KEYBOARD_MODE_CHANGE_DELAY_MS = 300;
    private static final int KEYBOARD_HIDE_DELAY_MS = 150;

    private static final int ACTION_BUTTON_TOUCH_OVERFLOW_LEFT = 15;

    private View mFirstVisibleFocusedView;
    private View mUrlBar;
    private StatusView mStatusView;
    private View mIconView;

    private Runnable mKeyboardResizeModeTask;

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
        updateUrlBarPaddingForSearchEngineIcon();
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
            updateUrlBarPaddingForSearchEngineIcon();
        }

        // This branch will be hit if the search engine logo experiment is enabled and we should
        // show the logo.
        if (shouldShowSearchEngineLogo) {
            mStatusView = findViewById(R.id.location_bar_status);
            mStatusView.updateSearchEngineStatusIcon(
                    shouldShowSearchEngineLogo, isSearchEngineGoogle, searchEngineUrl);
            mIconView = mStatusView.findViewById(R.id.location_bar_status_icon);
            mFirstVisibleFocusedView = mStatusView;
            updateUrlBarPaddingForSearchEngineIcon();

            // When the search engine icon is enabled, icons are translations into the parent view's
            // padding area. Set clip padding to false to prevent them from getting clipped.
            setClipToPadding(false);
        }
        setShowIconsWhenUrlFocused(shouldShowSearchEngineLogo);
    }

    /**
     * Factor in extra padding added for the focused state when the search engine icon is active.
     */
    private void updateUrlBarPaddingForSearchEngineIcon() {
        if (mUrlBar == null || mStatusView == null) return;

        // TODO(crbug.com/1019019): Come up with a better solution for M80 or M81.
        int endPadding = 0;
        if (SearchEngineLogoUtils.shouldShowSearchEngineLogo(mToolbarDataProvider.isIncognito())
                && hasFocus()) {
            // This padding prevents the UrlBar's content from extending past the available space
            // and into the next view while focused.
            endPadding = mStatusView.getEndPaddingPixelSizeForFocusState(true)
                    - mStatusView.getEndPaddingPixelSizeForFocusState(false);
        }

        mUrlBar.setPaddingRelative(mUrlBar.getPaddingStart(), mUrlBar.getPaddingTop(), endPadding,
                mUrlBar.getPaddingBottom());
    }

    /**
     * @return The first view visible when the location bar is focused.
     */
    public View getFirstViewVisibleWhenFocused() {
        return mFirstVisibleFocusedView;
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
        if (mStatusView == null) return 0;

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
        int offset = mStatusViewCoordinator.getStatusIconWidth() - getAdditionalOffsetForNTP();
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
        if (mStatusView == null) return 0;

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
        float translation = urlExpansionPercent
                * (mStatusView.getEndPaddingPixelSizeForFocusState(true)
                        - mStatusView.getEndPaddingPixelSizeForFocusState(false));

        if (!hasFocus && mIconView.getVisibility() == VISIBLE
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
                            * (mStatusViewCoordinator.getStatusIconWidth()
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
        mStatusViewCoordinator.setUrlFocusChangePercent(percent);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            // Remove the focus of this view once the URL field has taken focus as this view no
            // longer needs it.
            setFocusable(false);
            setFocusableInTouchMode(false);
        }
        updateUrlBarPaddingForSearchEngineIcon();
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

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change.  This will be called after any animations are performed to transition from one
     * focus state to the other.
     * @param hasFocus Whether the URL field has gained focus.
     */
    public void finishUrlFocusChange(boolean hasFocus) {
        if (!hasFocus) {
            // The animation rendering may not yet be 100% complete and hiding the keyboard makes
            // the animation quite choppy.
            postDelayed(new Runnable() {
                @Override
                public void run() {
                    getWindowAndroid().getKeyboardDelegate().hideKeyboard(mUrlBar);
                }
            }, KEYBOARD_HIDE_DELAY_MS);
            // Convert the keyboard back to resize mode (delay the change for an arbitrary amount
            // of time in hopes the keyboard will be completely hidden before making this change).
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE, true);
            mUrlActionContainer.setVisibility(GONE);
        } else {
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN, false);
            getWindowAndroid().getKeyboardDelegate().showKeyboard(mUrlBar);
        }
        updateUrlBarPaddingForSearchEngineIcon();
        mStatusViewCoordinator.onUrlAnimationFinished(hasFocus);
        setUrlFocusChangeInProgress(false);
        updateShouldAnimateIconChanges();
    }

    @Override
    protected void updateButtonVisibility() {
        super.updateButtonVisibility();
        updateMicButtonVisibility(mUrlFocusChangePercent);
    }

    @Override
    public void updateShouldAnimateIconChanges() {
        notifyShouldAnimateIconChanges(isUrlBarFocused() || isUrlFocusChangeInProgress());
    }

    @Override
    public void setShowIconsWhenUrlFocused(boolean showIcon) {
        super.setShowIconsWhenUrlFocused(showIcon);
        mStatusViewCoordinator.setShowIconsWhenUrlFocused(showIcon);
    }

    /**
     * @param softInputMode The software input resize mode.
     * @param delay Delay the change in input mode.
     */
    private void setSoftInputMode(final int softInputMode, boolean delay) {
        final WindowDelegate delegate = getWindowDelegate();

        if (mKeyboardResizeModeTask != null) {
            removeCallbacks(mKeyboardResizeModeTask);
            mKeyboardResizeModeTask = null;
        }

        if (delegate == null || delegate.getWindowSoftInputMode() == softInputMode) return;

        if (delay) {
            mKeyboardResizeModeTask = new Runnable() {
                @Override
                public void run() {
                    delegate.setWindowSoftInputMode(softInputMode);
                    mKeyboardResizeModeTask = null;
                }
            };
            postDelayed(mKeyboardResizeModeTask, KEYBOARD_MODE_CHANGE_DELAY_MS);
        } else {
            delegate.setWindowSoftInputMode(softInputMode);
        }
    }

    private int getAdditionalOffsetForNTP() {
        return getResources().getDimensionPixelSize(R.dimen.sei_search_box_lateral_padding)
                - getResources().getDimensionPixelSize(R.dimen.sei_location_bar_lateral_padding);
    }

    @Override
    public void updateVisualsForState() {
        super.updateVisualsForState();
        boolean isIncognito = getToolbarDataProvider().isIncognito();
        boolean shouldShowSearchEngineLogo =
                SearchEngineLogoUtils.shouldShowSearchEngineLogo(isIncognito);
        setShowIconsWhenUrlFocused(shouldShowSearchEngineLogo);
        mFirstVisibleFocusedView = shouldShowSearchEngineLogo ? mStatusView : mUrlBar;

        updateStatusVisibility();
        updateUrlBarPaddingForSearchEngineIcon();
    }

    @Override
    public void onTabLoadingNTP(NewTabPage ntp) {
        super.onTabLoadingNTP(ntp);
        updateStatusVisibility();
    }

    /** Update the status visibility according to the current state held in LocationBar. */
    private void updateStatusVisibility() {
        boolean incognito = getToolbarDataProvider().isIncognito();
        if (!SearchEngineLogoUtils.shouldShowSearchEngineLogo(incognito)) {
            return;
        }

        if (SearchEngineLogoUtils.currentlyOnNTP(mToolbarDataProvider)) {
            mStatusViewCoordinator.setStatusIconShown(hasFocus());
        } else {
            mStatusViewCoordinator.setStatusIconShown(true);
        }
    }
}
