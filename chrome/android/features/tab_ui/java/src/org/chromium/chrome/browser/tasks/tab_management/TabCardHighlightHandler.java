// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.GONE;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider.getTabCardHighlightBackgroundTintList;
import static org.chromium.ui.animation.CommonAnimationsFactory.createFadeInAnimation;
import static org.chromium.ui.animation.CommonAnimationsFactory.createFadeOutAnimation;

import android.animation.Animator;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.animation.AnimationListeners;

/** Handles UI updates as a result of changing highlight state for a tab card. */
@NullMarked
public class TabCardHighlightHandler {
    private final AnimationHandler mHighlightAnimationHandler = new AnimationHandler();
    private final View mCardWrapper;

    /**
     * @param cardWrapper The view that shows the highlight.
     */
    public TabCardHighlightHandler(View cardWrapper) {
        mCardWrapper = cardWrapper;
    }

    /**
     * Sets the highlight state for the tab card.
     *
     * @param highlightState The new highlight state.
     * @param isIncognito Whether the tab is incognito.
     */
    public void maybeAnimateForHighlightState(
            @TabCardHighlightState int highlightState, boolean isIncognito) {
        Context context = mCardWrapper.getContext();
        Drawable gridCardHighlightDrawable =
                highlightState == TabCardHighlightState.NOT_HIGHLIGHTED
                        ? null
                        : UiUtils.getTintedDrawable(
                                context,
                                R.drawable.tab_grid_card_highlight,
                                getTabCardHighlightBackgroundTintList(context, isIncognito));

        if (highlightState != TabCardHighlightState.HIGHLIGHTED) {
            mHighlightAnimationHandler.startAnimation(
                    highlightState == TabCardHighlightState.TO_BE_HIGHLIGHTED
                            ? buildFadeInAnimation(assumeNonNull(gridCardHighlightDrawable))
                            : buildFadeOutAnimation());
        } else {
            mCardWrapper.setVisibility(View.VISIBLE);
            mCardWrapper.setBackground(assumeNonNull(gridCardHighlightDrawable));
        }
    }

    /** Clears the highlight on the tab card. */
    public void clearHighlight() {
        mCardWrapper.setBackground(null);
        mCardWrapper.setVisibility(GONE);
    }

    private Animator buildFadeInAnimation(Drawable gridCardHighlightDrawable) {
        Animator fadeInAnimation = createFadeInAnimation(mCardWrapper);
        fadeInAnimation.addListener(
                AnimationListeners.onAnimationStart(
                        () -> mCardWrapper.setBackground(gridCardHighlightDrawable)));
        return fadeInAnimation;
    }

    private Animator buildFadeOutAnimation() {
        Animator fadeOutAnimation = createFadeOutAnimation(mCardWrapper);
        fadeOutAnimation.addListener(
                AnimationListeners.onAnimationEnd(
                        () -> {
                            mCardWrapper.setBackground(null);
                            mCardWrapper.setAlpha(1f);
                        }));
        return fadeOutAnimation;
    }
}
