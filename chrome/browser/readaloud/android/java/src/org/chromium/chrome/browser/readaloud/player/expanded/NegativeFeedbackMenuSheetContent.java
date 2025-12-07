// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.readaloud.player.Colors;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.Feedback.NegativeFeedbackReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

@NullMarked
class NegativeFeedbackMenuSheetContent extends MenuSheetContent {
    private final Context mContext;

    // Contents
    private final FrameLayout mContainer;
    private final Menu mOptionsMenu;
    private final ObjectAnimator mOptionsMenuOut;
    @Nullable private InteractionHandler mInteractionHandler;

    public NegativeFeedbackMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController) {
        this(context, parent, bottomSheetController, LayoutInflater.from(context));
    }

    @VisibleForTesting
    NegativeFeedbackMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            LayoutInflater layoutInflater) {
        super(parent, bottomSheetController);
        mContext = context;

        Resources res = mContext.getResources();
        // Set up options menu
        mOptionsMenu = (Menu) layoutInflater.inflate(R.layout.readaloud_menu, null);
        mOptionsMenu.findViewById(R.id.readaloud_menu_footer).setVisibility(View.VISIBLE);
        mOptionsMenu.addItem(
                NegativeFeedbackReason.NOT_FACTUALLY_CORRECT.getValue(),
                /* iconId= */ 0,
                res.getString(R.string.readaloud_negative_feedback_not_factually_correct),
                /* header */ null,
                MenuItem.Action.NONE);
        mOptionsMenu.addItem(
                NegativeFeedbackReason.BAD_VOICE.getValue(),
                /* iconId= */ 0,
                res.getString(R.string.readaloud_negative_feedback_didnt_like_the_voice),
                /* header */ null,
                MenuItem.Action.NONE);
        mOptionsMenu.addItem(
                NegativeFeedbackReason.NOT_ENGAGING.getValue(),
                /* iconId= */ 0,
                res.getString(R.string.readaloud_negative_feedback_not_engaging_enough),
                /* header */ null,
                MenuItem.Action.NONE);
        mOptionsMenu.addItem(
                NegativeFeedbackReason.OFFENSIVE.getValue(),
                /* iconId= */ 0,
                res.getString(R.string.readaloud_negative_feedback_offensive_content),
                /* header */ null,
                MenuItem.Action.NONE);
        mOptionsMenu.addItem(
                NegativeFeedbackReason.TECHNICAL_ISSUE.getValue(),
                /* iconId= */ 0,
                res.getString(R.string.readaloud_negative_feedback_technical_issue),
                /* header */ null,
                MenuItem.Action.NONE);
        mOptionsMenu.addItem(
                NegativeFeedbackReason.OTHER.getValue(),
                /* iconId= */ 0,
                res.getString(R.string.readaloud_negative_feedback_other),
                /* header */ null,
                MenuItem.Action.NONE);
        mOptionsMenu.setItemClickHandler(this::onOptionsMenuClick);
        mOptionsMenu.addOnLayoutChangeListener(this::onOptionsMenuLayoutChange);
        mOptionsMenu.afterInflating(
                () -> {
                    // Views inside menu layout are only available after inflating.
                    mOptionsMenu.setTitle(R.string.readaloud_negative_feedback_menu_title);
                    mOptionsMenu.setBackPressHandler(this::onBackPressed);
                });

        // Put both in content view
        mContainer = new FrameLayout(context);
        mContainer.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT));
        mContainer.addView(mOptionsMenu);
        Colors.setBottomSheetContentBackground(mContainer);

        mOptionsMenuOut =
                ObjectAnimator.ofFloat(
                        mOptionsMenu, View.TRANSLATION_X, 0f, (float) -mOptionsMenu.getWidth());
    }

    private void onOptionsMenuLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        // Do nothing if options menu width is unchanged.
        if ((right - left) == (oldRight - oldLeft)) {
            return;
        }

        // Skip to the end of the animation if running, or just use animation to set X values if not
        // running.
        updateAnimationValues();
    }

    void setInteractionHandler(InteractionHandler interactionHandler) {
        mInteractionHandler = interactionHandler;
    }

    void onOrientationChange(int orientation) {
        mOptionsMenu.onOrientationChange(orientation);
    }

    @Override
    public View getContentView() {
        return mContainer;
    }

    @Override
    public int getVerticalScrollOffset() {
        return assumeNonNull(mOptionsMenu.getScrollView()).getScrollY();
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.readaloud_negative_feedback_menu_description);
    }

    private void onOptionsMenuClick(int itemId) {
        // TODO(crbug.com/401256755): Actually send the feedback on click.
        assumeNonNull(mInteractionHandler)
                .onNegativeFeedback(NegativeFeedbackReason.fromValue(itemId));
        mBottomSheetController.hideContent(this, true);
    }

    private void updateAnimationValues() {
        // Set start and end values for X translation.
        float optionsMenuWidth = (float) mOptionsMenu.getWidth();
        mOptionsMenuOut.setFloatValues(-optionsMenuWidth, 0f);
    }

    Menu getMenuForTesting() {
        return mOptionsMenu;
    }
}
