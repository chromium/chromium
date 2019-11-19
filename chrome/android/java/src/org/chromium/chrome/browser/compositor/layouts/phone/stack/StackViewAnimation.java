// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackAnimation.OverviewAnimationType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;

/**
 * A factory that builds Android view animations for the tab stack.
 */
public class StackViewAnimation {
    private static final int TAB_OPENED_BG_ANIMATION_DURATION = 150;
    private static final int TAB_OPENED_VIEW_ANIMATION_DURATION = 350;

    private final int mTranslationYStart;

    /**
     * Constructor.
     *
     * @param resources Android {@link Resources} used to retrieve dimensions.
     */
    public StackViewAnimation(Resources resources) {
        mTranslationYStart =
                resources.getDimensionPixelSize(R.dimen.open_new_tab_animation_y_translation);
    }

    /**
     * The wrapper method responsible for delegating animation requests to the appropriate helper
     * method.
     * @param type       The type of animation to be created.  This is what determines which helper
     *                   method is called.
     * @param tabs       The tabs that make up the current stack.
     * @param container  The {@link ViewGroup} that {@link View}s can be added to/removed from.
     * @param list       The {@link TabList} that this animation will influence.
     * @param focusIndex The index of the tab that is the focus of this animation.
     * @return           The resulting {@link Animator} that will animate the Android views.
     */
    public Animator createAnimatorForType(@OverviewAnimationType int type, StackTab[] tabs,
            ViewGroup container, TabList list, int focusIndex) {
        Animator animator = null;

        if (list != null && type == OverviewAnimationType.NEW_TAB_OPENED) {
            animator = createNewTabOpenedAnimator(tabs, container, list, focusIndex);
        }

        return animator;
    }

    private Animator createNewTabOpenedAnimator(
            StackTab[] tabs, ViewGroup container, TabList list, int focusIndex) {
        Tab tab = list.getTabAt(focusIndex);
        if (tab == null || !tab.isNativePage()) return null;

        View view = tab.getView();
        if (view == null) return null;

        // Set up the view hierarchy
        if (view.getParent() != null) ((ViewGroup) view.getParent()).removeView(view);
        ViewGroup bgView = new FrameLayout(view.getContext());
        bgView.setBackgroundColor(TabThemeColorHelper.getBackgroundColor(tab));
        bgView.addView(view);
        container.addView(
                bgView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        // Update any compositor state that needs to change
        if (tabs != null && focusIndex >= 0 && focusIndex < tabs.length) {
            tabs[focusIndex].setAlpha(0.f);
        }

        // Build the view animations
        PropertyValuesHolder viewAlpha = PropertyValuesHolder.ofFloat(View.ALPHA, 0.f, 1.f);
        ObjectAnimator viewAlphaAnimator = ObjectAnimator.ofPropertyValuesHolder(view, viewAlpha);
        viewAlphaAnimator.setDuration(TAB_OPENED_VIEW_ANIMATION_DURATION);
        viewAlphaAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        PropertyValuesHolder yTranslation =
                PropertyValuesHolder.ofFloat(View.TRANSLATION_Y, mTranslationYStart, 0.f);
        ObjectAnimator viewYTranslationAnimator =
                ObjectAnimator.ofPropertyValuesHolder(view, yTranslation);
        viewYTranslationAnimator.setDuration(TAB_OPENED_VIEW_ANIMATION_DURATION);
        viewYTranslationAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        PropertyValuesHolder bgAlpha = PropertyValuesHolder.ofFloat(View.ALPHA, 0.f, 1.f);
        ObjectAnimator bgAlphaAnimator = ObjectAnimator.ofPropertyValuesHolder(bgView, bgAlpha);
        bgAlphaAnimator.setDuration(TAB_OPENED_BG_ANIMATION_DURATION);
        bgAlphaAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        AnimatorSet set = new AnimatorSet();
        set.playTogether(viewAlphaAnimator, viewYTranslationAnimator, bgAlphaAnimator);

        return set;
    }
}
