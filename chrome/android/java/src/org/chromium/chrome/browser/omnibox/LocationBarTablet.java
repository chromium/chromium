// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Property;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
import org.chromium.chrome.browser.ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.util.ArrayList;
import java.util.List;

/**
 * Location bar for tablet form factors.
 */
public class LocationBarTablet extends LocationBarLayout {
    private static final int KEYBOARD_MODE_CHANGE_DELAY_MS = 300;
    private static final long MAX_NTP_KEYBOARD_FOCUS_DURATION_MS = 200;

    private static final int ICON_FADE_ANIMATION_DURATION_MS = 150;
    private static final int ICON_FADE_ANIMATION_DELAY_MS = 75;
    private static final int WIDTH_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int WIDTH_CHANGE_ANIMATION_DELAY_MS = 75;

    private final Property<LocationBarTablet, Float> mUrlFocusChangePercentProperty =
            new Property<LocationBarTablet, Float>(Float.class, "") {
                @Override
                public Float get(LocationBarTablet object) {
                    return object.mUrlFocusChangePercent;
                }

                @Override
                public void set(LocationBarTablet object, Float value) {
                    setUrlFocusChangePercent(value);
                }
            };

    private final Property<LocationBarTablet, Float> mWidthChangePercentProperty =
            new Property<LocationBarTablet, Float>(Float.class, "") {
                @Override
                public Float get(LocationBarTablet object) {
                    return object.mWidthChangePercent;
                }

                @Override
                public void set(LocationBarTablet object, Float value) {
                    setWidthChangeAnimationPercent(value);
                }
            };

    private final Runnable mKeyboardResizeModeTask = new Runnable() {
        @Override
        public void run() {
            getWindowDelegate().setWindowSoftInputMode(
                    WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }
    };

    private View mLocationBarIcon;
    private View mBookmarkButton;
    private View mSaveOfflineButton;
    private Animator mUrlFocusChangeAnimator;
    private View[] mTargets;
    private final Rect mCachedTargetBounds = new Rect();

    // Whether the microphone and bookmark buttons should be shown in the location bar. These
    // buttons are hidden if the window size is < 600dp.
    private boolean mShouldShowButtonsWhenUnfocused;

    // Variables needed for animating the location bar and toolbar buttons hiding/showing.
    private final int mToolbarButtonsWidth;
    private final int mMicButtonWidth;
    private boolean mAnimatingWidthChange;
    private float mWidthChangePercent;
    private float mLayoutLeft;
    private float mLayoutRight;
    private int mToolbarStartPaddingDifference;

    /**
     * Constructor used to inflate from XML.
     */
    public LocationBarTablet(Context context, AttributeSet attrs) {
        super(context, attrs);
        mShouldShowButtonsWhenUnfocused = true;

        mToolbarButtonsWidth = getResources().getDimensionPixelOffset(R.dimen.toolbar_button_width)
                * ToolbarTablet.HIDEABLE_BUTTON_COUNT;
        mMicButtonWidth = getResources().getDimensionPixelOffset(R.dimen.location_bar_icon_width);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mLocationBarIcon = findViewById(R.id.location_bar_status_icon);
        mBookmarkButton = findViewById(R.id.bookmark_button);
        mSaveOfflineButton = findViewById(R.id.save_offline_button);

        mTargets = new View[] {mUrlBar, mDeleteButton};
        mStatusViewCoordinator.setShowIconsWhenUrlFocused(true);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mTargets == null) return true;

        View selectedTarget = null;
        float selectedDistance = 0;
        // newX and newY are in the coordinates of the selectedTarget.
        float newX = 0;
        float newY = 0;
        for (View target : mTargets) {
            if (!target.isShown()) continue;

            mCachedTargetBounds.set(0, 0, target.getWidth(), target.getHeight());
            offsetDescendantRectToMyCoords(target, mCachedTargetBounds);
            float x = event.getX();
            float y = event.getY();
            float dx = distanceToRange(mCachedTargetBounds.left, mCachedTargetBounds.right, x);
            float dy = distanceToRange(mCachedTargetBounds.top, mCachedTargetBounds.bottom, y);
            float distance = Math.abs(dx) + Math.abs(dy);
            if (selectedTarget == null || distance < selectedDistance) {
                selectedTarget = target;
                selectedDistance = distance;
                newX = x + dx;
                newY = y + dy;
            }
        }

        if (selectedTarget == null) return false;

        event.setLocation(newX, newY);
        return selectedTarget.onTouchEvent(event);
    }

    // Returns amount by which to adjust to move value inside the given range.
    private static float distanceToRange(float min, float max, float value) {
        return value < min ? (min - value) : value > max ? (max - value) : 0;
    }

    @Override
    public void handleUrlFocusAnimation(final boolean hasFocus) {
        super.handleUrlFocusAnimation(hasFocus);

        removeCallbacks(mKeyboardResizeModeTask);

        if (mUrlFocusChangeAnimator != null && mUrlFocusChangeAnimator.isRunning()) {
            mUrlFocusChangeAnimator.cancel();
            mUrlFocusChangeAnimator = null;
        }

        if (getToolbarDataProvider().getNewTabPageForCurrentTab() == null) {
            finishUrlFocusChange(hasFocus);
            return;
        }

        Rect rootViewBounds = new Rect();
        getRootView().getLocalVisibleRect(rootViewBounds);
        float screenSizeRatio = (rootViewBounds.height()
                / (float) (Math.max(rootViewBounds.height(), rootViewBounds.width())));
        mUrlFocusChangeAnimator =
                ObjectAnimator.ofFloat(this, mUrlFocusChangePercentProperty, hasFocus ? 1f : 0f);
        mUrlFocusChangeAnimator.setDuration(
                (long) (MAX_NTP_KEYBOARD_FOCUS_DURATION_MS * screenSizeRatio));
        mUrlFocusChangeAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animator) {
                finishUrlFocusChange(hasFocus);
            }

            @Override
            public void onCancel(Animator animator) {
                setUrlFocusChangeInProgress(false);
            }
        });
        setUrlFocusChangeInProgress(true);
        mUrlFocusChangeAnimator.start();
    }

    private void finishUrlFocusChange(boolean hasFocus) {
        // Report focus change early to trigger animations.
        mStatusViewCoordinator.onUrlFocusChange(hasFocus);
        if (hasFocus) {
            if (getWindowDelegate().getWindowSoftInputMode()
                    != WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN) {
                getWindowDelegate().setWindowSoftInputMode(
                        WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
            }
            getWindowAndroid().getKeyboardDelegate().showKeyboard(mUrlBar);
        } else {
            getWindowAndroid().getKeyboardDelegate().hideKeyboard(mUrlBar);
            // Convert the keyboard back to resize mode (delay the change for an arbitrary
            // amount of time in hopes the keyboard will be completely hidden before making
            // this change).
            if (getWindowDelegate().getWindowSoftInputMode()
                    != WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE) {
                postDelayed(mKeyboardResizeModeTask, KEYBOARD_MODE_CHANGE_DELAY_MS);
            }
        }
        setUrlFocusChangeInProgress(false);
    }

    /**
     * @param shouldShowButtons Whether buttons should be displayed in the URL bar when it's not
     *                          focused.
     */
    public void setShouldShowButtonsWhenUnfocused(boolean shouldShowButtons) {
        mShouldShowButtonsWhenUnfocused = shouldShowButtons;
        updateButtonVisibility();
    }

    /**
     * Updates percentage of current the URL focus change animation.
     * @param percent 1.0 is 100% focused, 0 is completely unfocused.
     */
    private void setUrlFocusChangePercent(float percent) {
        mUrlFocusChangePercent = percent;

        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (ntp != null) ntp.setUrlFocusChangeAnimationPercent(percent);
    }

    @Override
    public void updateButtonVisibility() {
        super.updateButtonVisibility();

        boolean showBookmarkButton =
                mShouldShowButtonsWhenUnfocused && shouldShowPageActionButtons();
        mBookmarkButton.setVisibility(showBookmarkButton ? View.VISIBLE : View.GONE);

        boolean showSaveOfflineButton =
                mShouldShowButtonsWhenUnfocused && shouldShowSaveOfflineButton();
        mSaveOfflineButton.setVisibility(showSaveOfflineButton ? View.VISIBLE : View.GONE);
        if (showSaveOfflineButton) mSaveOfflineButton.setEnabled(isSaveOfflineButtonEnabled());

        if (!mShouldShowButtonsWhenUnfocused) {
            updateMicButtonVisibility(mUrlFocusChangePercent);
        } else {
            mMicButton.setVisibility(shouldShowMicButton() ? View.VISIBLE : View.GONE);
        }
    }

    @Override
    public void onSuggestionsHidden() {
        super.onSuggestionsHidden();
        mStatusViewCoordinator.setFirstSuggestionIsSearchType(false);
    }

    @Override
    public void onSuggestionsChanged(String autocompleteText) {
        super.onSuggestionsChanged(autocompleteText);
        mStatusViewCoordinator.setFirstSuggestionIsSearchType(
                mAutocompleteCoordinator.getSuggestionCount() > 0
                && !mAutocompleteCoordinator.getSuggestionAt(0).isUrlSuggestion());
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int measuredWidth = getMeasuredWidth();

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (getMeasuredWidth() != measuredWidth) {
            setUnfocusedWidth(getMeasuredWidth());
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        mLayoutLeft = left;
        mLayoutRight = right;

        if (mAnimatingWidthChange) {
            setWidthChangeAnimationPercent(mWidthChangePercent);
        }
    }

    /**
     * @param button The {@link View} of the button to show.
     * @return An animator to run for the given view when showing buttons in the unfocused location
     *         bar. This should also be used to create animators for showing toolbar buttons.
     */
    public ObjectAnimator createShowButtonAnimator(View button) {
        if (button.getVisibility() != View.VISIBLE) {
            button.setAlpha(0.f);
        }
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(button, View.ALPHA, 1.f);
        buttonAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        buttonAnimator.setStartDelay(ICON_FADE_ANIMATION_DELAY_MS);
        buttonAnimator.setDuration(ICON_FADE_ANIMATION_DURATION_MS);
        return buttonAnimator;
    }

    /**
     * @param button The {@link View} of the button to hide.
     * @return An animator to run for the given view when hiding buttons in the unfocused location
     *         bar. This should also be used to create animators for hiding toolbar buttons.
     */
    public ObjectAnimator createHideButtonAnimator(View button) {
        ObjectAnimator buttonAnimator = ObjectAnimator.ofFloat(button, View.ALPHA, 0.f);
        buttonAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        buttonAnimator.setDuration(ICON_FADE_ANIMATION_DURATION_MS);
        return buttonAnimator;
    }

    /**
     * Creates animators for showing buttons in the unfocused location bar. The buttons fade in
     * while width of the location bar gets smaller. There are toolbar buttons that also show at
     * the same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding between
     *                                      the beginning and end of the animation.
     * @return An ArrayList of animators to run.
     */
    public List<Animator> getShowButtonsWhenUnfocusedAnimators(int toolbarStartPaddingDifference) {
        mToolbarStartPaddingDifference = toolbarStartPaddingDifference;

        ArrayList<Animator> animators = new ArrayList<>();

        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mWidthChangePercentProperty, 0f);
        widthChangeAnimator.setDuration(WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        widthChangeAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mAnimatingWidthChange = true;
                setShouldShowButtonsWhenUnfocused(true);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                // Only reset values if the animation is ending because it's completely finished
                // and not because it was canceled.
                if (mWidthChangePercent == 0.f) {
                    mAnimatingWidthChange = false;
                    resetValuesAfterAnimation();
                }
            }
        });
        animators.add(widthChangeAnimator);

        // When buttons show in the unfocused location bar, either the delete button or bookmark
        // button will be showing. If the delete button is currently showing, the bookmark button
        // should not fade in.
        if (mDeleteButton.getVisibility() != View.VISIBLE) {
            animators.add(createShowButtonAnimator(mBookmarkButton));
        }

        if (shouldShowSaveOfflineButton()) {
            animators.add(createShowButtonAnimator(mSaveOfflineButton));
        } else if (mMicButton.getVisibility() != View.VISIBLE || mMicButton.getAlpha() != 1.f) {
            // If the microphone button is already fully visible, don't animate its appearance.
            animators.add(createShowButtonAnimator(mMicButton));
        }

        return animators;
    }

    /**
     * Creates animators for hiding buttons in the unfocused location bar. The buttons fade out
     * while width of the location bar gets larger. There are toolbar buttons that also hide at the
     * same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding between
     *                                      the beginning and end of the animation.
     * @return An ArrayList of animators to run.
     */
    public List<Animator> getHideButtonsWhenUnfocusedAnimators(int toolbarStartPaddingDifference) {
        mToolbarStartPaddingDifference = toolbarStartPaddingDifference;

        ArrayList<Animator> animators = new ArrayList<>();

        Animator widthChangeAnimator =
                ObjectAnimator.ofFloat(this, mWidthChangePercentProperty, 1f);
        widthChangeAnimator.setStartDelay(WIDTH_CHANGE_ANIMATION_DELAY_MS);
        widthChangeAnimator.setDuration(WIDTH_CHANGE_ANIMATION_DURATION_MS);
        widthChangeAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        widthChangeAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mAnimatingWidthChange = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                // Only reset values if the animation is ending because it's completely finished
                // and not because it was canceled.
                if (mWidthChangePercent == 1.f) {
                    mAnimatingWidthChange = false;
                    resetValuesAfterAnimation();
                    setShouldShowButtonsWhenUnfocused(false);
                }
            }
        });
        animators.add(widthChangeAnimator);

        // When buttons show in the unfocused location bar, either the delete button or bookmark
        // button will be showing. If the delete button is currently showing, the bookmark button
        // should not fade out.
        if (mDeleteButton.getVisibility() != View.VISIBLE) {
            animators.add(createHideButtonAnimator(mBookmarkButton));
        }

        if (shouldShowSaveOfflineButton() && mSaveOfflineButton.getVisibility() == View.VISIBLE) {
            animators.add(createHideButtonAnimator(mSaveOfflineButton));
        } else if (!(mUrlBar.isFocused() && mDeleteButton.getVisibility() != View.VISIBLE)) {
            // If the save offline button isn't enabled, the microphone button always shows when
            // buttons are shown in the unfocused location bar. When buttons are hidden in the
            // unfocused location bar, the microphone shows if the location bar is focused and the
            // delete button isn't showing. The microphone button should not be hidden if the
            // url bar is currently focused and the delete button isn't showing.
            animators.add(createHideButtonAnimator(mMicButton));
        }

        return animators;
    }

    /**
     * Resets the alpha and translation X for all views affected by the animations for showing or
     * hiding buttons.
     */
    private void resetValuesAfterAnimation() {
        mMicButton.setTranslationX(0);
        mDeleteButton.setTranslationX(0);
        mBookmarkButton.setTranslationX(0);
        mSaveOfflineButton.setTranslationX(0);
        mLocationBarIcon.setTranslationX(0);
        mUrlBar.setTranslationX(0);

        mMicButton.setAlpha(1.f);
        mDeleteButton.setAlpha(1.f);
        mBookmarkButton.setAlpha(1.f);
        mSaveOfflineButton.setAlpha(1.f);
    }

    /**
     * Updates completion percentage for the location bar width change animation.
     * @param percent How complete the animation is, where 0 represents the normal width (toolbar
     *                buttons fully visible) and 1.f represents the expanded width (toolbar buttons
     *                fully hidden).
     */
    private void setWidthChangeAnimationPercent(float percent) {
        mWidthChangePercent = percent;

        float offset = (mToolbarButtonsWidth + mToolbarStartPaddingDifference) * percent;

        if (LocalizationUtils.isLayoutRtl()) {
            // The location bar's right edge is its regular layout position when toolbar buttons are
            // completely visible and its layout position + mToolbarButtonsWidth when toolbar
            // buttons are completely hidden.
            setRight((int) (mLayoutRight + offset));
        } else {
            // The location bar's left edge is it's regular layout position when toolbar buttons are
            // completely visible and its layout position - mToolbarButtonsWidth when they are
            // completely hidden.
            setLeft((int) (mLayoutLeft - offset));
        }

        // As the location bar's right edge moves right (increases) or left edge moves left
        // (decreases), the child views' translation X increases, keeping them visually in the same
        // location for the duration of the animation.
        int deleteOffset = (int) (mMicButtonWidth * percent);
        setChildTranslationsForWidthChangeAnimation((int) offset, deleteOffset);
    }

    /**
     * Sets the translation X values for child views during the width change animation. This
     * compensates for the change to the left/right position of the location bar and ensures child
     * views stay in the same spot visually during the animation.
     *
     * The delete button is special because if it's visible during the animation its start and end
     * location are not the same. When buttons are shown in the unfocused location bar, the delete
     * button is left of the microphone. When buttons are not shown in the unfocused location bar,
     * the delete button is aligned with the left edge of the location bar.
     *
     * @param offset The offset to use for the child views.
     * @param deleteOffset The additional offset to use for the delete button.
     */
    private void setChildTranslationsForWidthChangeAnimation(int offset, int deleteOffset) {
        if (getLayoutDirection() != LAYOUT_DIRECTION_RTL) {
            // When the location bar layout direction is LTR, the buttons at the end (left side)
            // of the location bar need to stick to the left edge.
            if (mSaveOfflineButton.getVisibility() == View.VISIBLE) {
                mSaveOfflineButton.setTranslationX(offset);
            } else {
                mMicButton.setTranslationX(offset);
            }

            if (mDeleteButton.getVisibility() == View.VISIBLE) {
                mDeleteButton.setTranslationX(offset + deleteOffset);
            } else {
                mBookmarkButton.setTranslationX(offset);
            }
        } else {
            // When the location bar layout direction is RTL, the location bar icon and url
            // container at the start (right side) of the location bar need to stick to the right
            // edge.
            mLocationBarIcon.setTranslationX(offset);
            mUrlBar.setTranslationX(offset);

            if (mDeleteButton.getVisibility() == View.VISIBLE) {
                mDeleteButton.setTranslationX(-deleteOffset);
            }
        }
    }

    private boolean shouldShowSaveOfflineButton() {
        if (!mNativeInitialized || mToolbarDataProvider == null) return false;
        Tab tab = mToolbarDataProvider.getTab();
        if (tab == null) return false;
        // The save offline button should not be shown on native pages. Currently, trying to
        // save an offline page in incognito crashes, so don't show it on incognito either.
        return shouldShowPageActionButtons() && !tab.isIncognito();
    }

    private boolean isSaveOfflineButtonEnabled() {
        if (mToolbarDataProvider == null) return false;
        return DownloadUtils.isAllowedToDownloadPage(mToolbarDataProvider.getTab());
    }

    private boolean shouldShowPageActionButtons() {
        if (!mNativeInitialized) return true;

        // There are two actions, bookmark and save offline, and they should be shown if the
        // omnibox isn't focused.
        return !(mUrlBar.hasFocus() || mUrlFocusChangeInProgress);
    }

    private boolean shouldShowMicButton() {
        // If the download UI is enabled, the mic button should be only be shown when the url bar
        // is focused.
        return mVoiceRecognitionHandler != null && mVoiceRecognitionHandler.isVoiceSearchEnabled()
                && mNativeInitialized && (mUrlBar.hasFocus() || mUrlFocusChangeInProgress);
    }
}
