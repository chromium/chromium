// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.tablet.emptybackground;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.tablet.emptybackground.incognitotoggle.IncognitoToggleButtonTablet;
import org.chromium.ui.KeyboardVisibilityDelegate;

/**
 * Represents the background screen that shows when no tabs are visible.  This {@link View}
 * handles making itself invisible or not automatically depending on the state of the system.
 */
public class EmptyBackgroundViewTablet extends FrameLayout {
    private static final int ANIMATE_DURATION_MS = 200;

    private TabModelSelector mTabModelSelector;
    private TabCreator mTabCreator;

    private Animator mCurrentTransitionAnimation;

    private Animator mAnimateInAnimation;
    private Animator mAnimateOutAnimation;

    private IncognitoToggleButtonTablet mIncognitoToggleButton;

    /**
     * Creates an instance of {@link EmptyBackgroundViewTablet}.
     * @param context The {@link Context} to create this {@link View} under.
     * @param attrs An {@link AttributeSet} that contains information on how to build this
     *         {@link View}.
     */
    public EmptyBackgroundViewTablet(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        View newTabButton = findViewById(R.id.empty_new_tab_button);
        newTabButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mTabCreator == null) return;
                mTabModelSelector.getModel(false).commitAllTabClosures();
                mTabCreator.launchNTP();
            }
        });

        buildAnimatorSets();
    }

    /**
     * Sets the {@link TabModelSelector} that will be queried for information about the state of
     * the system.
     * @param tabModelSelector A {@link TabModelSelector} that represents the state of the system.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;

        mIncognitoToggleButton = findViewById(R.id.empty_incognito_toggle_button);

        mIncognitoToggleButton.setTabModelSelector(mTabModelSelector);
    }

    /**
     * Sets the {@link TabCreator} that will be used to open the New Tab Page.
     * @param tabCreator A {@link TabCreator} to open the New Tab Page.
     */
    public void setTabCreator(TabCreator tabCreator) {
        mTabCreator = tabCreator;
    }

    /**
     * Creates an on touch listener for the menu button using the given menu handler.
     * @param menuHandler The menu handler to be used for showing the pop up menu.
     */
    public void setMenuOnTouchListener(final AppMenuHandler menuHandler) {
        final ImageButton menuBtn = findViewById(R.id.empty_menu_button);
        final AppMenuButtonHelper menuPopupButtonHelper = menuHandler.createAppMenuButtonHelper();
        menuBtn.setOnTouchListener(menuPopupButtonHelper);
        menuPopupButtonHelper.setOnAppMenuShownListener(
                () -> RecordUserAction.record("MobileToolbarShowMenu"));
    }

    public void setEmptyContainerState(boolean shouldShow) {
        Animator nextAnimator = null;

        if (shouldShow && getVisibility() != View.VISIBLE
                && mCurrentTransitionAnimation != mAnimateInAnimation) {
            nextAnimator = mAnimateInAnimation;
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(this);
        } else if (!shouldShow && getVisibility() != View.GONE
                && mCurrentTransitionAnimation != mAnimateOutAnimation) {
            nextAnimator = mAnimateOutAnimation;
        }

        if (nextAnimator != null) {
            if (mCurrentTransitionAnimation != null) mCurrentTransitionAnimation.cancel();
            mCurrentTransitionAnimation = nextAnimator;
            mCurrentTransitionAnimation.start();
        }
    }

    private void buildAnimatorSets() {
        TypedArray a = getContext().getTheme().obtainStyledAttributes(
                R.style.ToolbarButton, new int[] {android.R.attr.layout_height});
        int viewHeight = a.getDimensionPixelSize(0, 0);
        a.recycle();
        View view = findViewById(R.id.empty_layout_button_container);

        // Build the in animation
        mAnimateInAnimation = ObjectAnimator.ofFloat(view, View.TRANSLATION_Y, -viewHeight, 0.f);
        mAnimateInAnimation.setDuration(ANIMATE_DURATION_MS);

        mAnimateInAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                setVisibility(View.VISIBLE);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mCurrentTransitionAnimation = null;
                getRootView().findViewById(R.id.control_container).setVisibility(INVISIBLE);
            }
        });

        // Build the out animation
        mAnimateOutAnimation = ObjectAnimator.ofFloat(view, View.TRANSLATION_Y, 0.f, -viewHeight);
        mAnimateOutAnimation.setDuration(ANIMATE_DURATION_MS);

        mAnimateOutAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                setVisibility(View.VISIBLE);
                getRootView().findViewById(R.id.control_container).setVisibility(VISIBLE);
                // Disable the incognito toggle button while the tab switcher animation is running
                // to avoid getting into a weird UI state if the button is clicked multiple times.
                // See crbug.com/586875.
                mIncognitoToggleButton.setEnabled(false);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                setVisibility(View.GONE);
                mCurrentTransitionAnimation = null;
                mIncognitoToggleButton.setEnabled(true);
            }
        });
    }
}
