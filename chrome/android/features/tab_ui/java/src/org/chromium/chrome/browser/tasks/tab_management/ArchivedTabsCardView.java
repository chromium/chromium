// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.BASE_ANIMATION_DURATION_MS;
import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.animation.AnimationHandler;

/**
 * Represents a message card view in the Grid Tab Switcher for showing information about archived
 * tabs.
 */
@NullMarked
public class ArchivedTabsCardView extends FrameLayout {
    private final AnimationHandler mAnimationHandler = new AnimationHandler();

    private View mCardContainer;
    private TextView mTitleView;
    private TextView mSubtitleView;
    private View mEndIconView;

    public ArchivedTabsCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mCardContainer = findViewById(R.id.card);
        mTitleView = findViewById(R.id.title);
        mSubtitleView = findViewById(R.id.subtitle);
        setSubtitleText();
        mEndIconView = findViewById(R.id.end_image);

        mEndIconView.setScaleX(isLayoutRtl() ? -1 : 1);
        GradientDrawable cardViewBg = (GradientDrawable) mCardContainer.getBackground().mutate();
        cardViewBg.setColor(SemanticColorUtils.getCardBackgroundColor(getContext()));
    }

    public void setIconHighlight(boolean isHighlighted) {
        if (isHighlighted) {
            showHighlight();
        } else {
            hideHighlight();
        }
    }

    /** Shows a highlight on the end icon. */
    private void showHighlight() {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(false);
        ViewHighlighter.turnOnHighlight(mEndIconView, params);
    }

    /** Hides the highlight on the end icon. */
    private void hideHighlight() {
        ViewHighlighter.turnOffHighlight(mEndIconView);
    }

    /**
     * Sets the title text based on the number of inactive tabs.
     *
     * @param numInactiveTabs The number of tabs that are archived.
     */
    public void setNumberOfArchivedTabs(int numInactiveTabs) {
        String title =
                getResources()
                        .getQuantityString(
                                R.plurals.archived_tab_card_title,
                                numInactiveTabs,
                                numInactiveTabs);
        mTitleView.setText(title);
    }

    /**
     * Sets the click handler for the entire card.
     *
     * @param handler The {@link Runnable} to execute on click.
     */
    public void setClickHandler(Runnable handler) {
        setOnClickListener(v -> handler.run());
    }

    /**
     * Sets the width of the card container.
     *
     * @param width The width in pixels.
     */
    public void setCardWidth(@Px int width) {
        var params = mCardContainer.getLayoutParams();
        params.width = width;
        mCardContainer.setLayoutParams(params);
    }

    /**
     * Scales the card view in or out.
     *
     * @param status The type of scaling to perform.
     */
    void scaleCard(@AnimationStatus int status) {
        boolean isZoomIn = status == AnimationStatus.HOVERED_CARD_ZOOM_IN;
        boolean isZoomOut = status == AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        if (!isZoomOut && !isZoomIn) return;

        float finalScale = isZoomIn ? 0.8f : 1f;

        AnimatorSet scaleAnimator = new AnimatorSet();
        ObjectAnimator scaleX = ObjectAnimator.ofFloat(this, View.SCALE_X, finalScale);
        ObjectAnimator scaleY = ObjectAnimator.ofFloat(this, View.SCALE_Y, finalScale);
        scaleX.setDuration(BASE_ANIMATION_DURATION_MS);
        scaleY.setDuration(BASE_ANIMATION_DURATION_MS);
        scaleAnimator.playTogether(scaleX, scaleY);

        mAnimationHandler.startAnimation(scaleAnimator);
    }

    /** Sets the subtitle text based on the archive time delta. */
    private void setSubtitleText() {
        mSubtitleView.setText(
                getResources()
                        .getString(
                                ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()
                                        ? R.string.archived_tab_card_subtitle_with_tab_groups
                                        : R.string.archived_tab_card_subtitle));
    }
}
