// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.scene;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.widget.RelativeLayout;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.content_creation.reactions.LightweightReactionsMediator;
import org.chromium.chrome.browser.content_creation.reactions.ReactionGifDrawable;
import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.chrome.browser.content_creation.reactions.toolbar.ToolbarReactionsDelegate;
import org.chromium.components.content_creation.reactions.ReactionMetadata;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.base.ViewUtils;

import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Manages the scene UI and the reactions on the scene.
 */
public class SceneCoordinator implements SceneEditorDelegate, ToolbarReactionsDelegate {
    private static final int DEFAULT_REACTION_SIZE_DP = 192;
    private static final int REACTION_OFFSET_DP = 45;
    private static final int MAX_REACTION_COUNT = 10;

    private final Activity mActivity;
    private final LightweightReactionsMediator mMediator;
    private final Set<ReactionLayout> mReactionLayouts;

    private ReactionLayout mActiveReaction;
    private RelativeLayout mSceneBackground;

    private int mNbReactionsAdded;
    private int mNbTypeChange;
    private int mNbRotateScale;
    private int mNbDuplicate;
    private int mNbDelete;
    private int mNbMove;

    /**
     * Constructs a new {@link SceneCoordinator}.
     *
     * @param activity The current {@link Activity}.
     */
    public SceneCoordinator(Activity activity, LightweightReactionsMediator mediator) {
        mActivity = activity;
        mMediator = mediator;
        mReactionLayouts = new HashSet<>();

        mNbReactionsAdded = 0;
        mNbTypeChange = 0;
        mNbRotateScale = 0;
        mNbDuplicate = 0;
        mNbDelete = 0;
        mNbMove = 0;
    }

    public void setSceneBackground(RelativeLayout sceneBackground) {
        mSceneBackground = sceneBackground;
        mSceneBackground.setOnClickListener((view) -> { clearSelection(); });
    }

    public void addReactionInDefaultLocation(ReactionMetadata reaction) {
        ++mNbReactionsAdded;
        mMediator.getGifForUrl(reaction.assetUrl, (baseGifImage) -> {
            if (mSceneBackground == null) {
                return;
            }

            ReactionGifDrawable drawable =
                    new ReactionGifDrawable(reaction, baseGifImage, Bitmap.Config.ARGB_8888);

            ReactionLayout reactionLayout = (ReactionLayout) LayoutInflaterUtils.inflate(
                    mActivity, R.layout.reaction_layout, null);
            reactionLayout.init(drawable, this, reaction.localizedName);

            int reactionSizePx = ViewUtils.dpToPx(mActivity, DEFAULT_REACTION_SIZE_DP);
            RelativeLayout.LayoutParams lp =
                    new RelativeLayout.LayoutParams(reactionSizePx, reactionSizePx);
            Resources res = mActivity.getResources();
            int screenWidth = res.getDisplayMetrics().widthPixels;
            int screenHeight = res.getDisplayMetrics().heightPixels;
            int leftPx = screenWidth / 2 - reactionSizePx / 2;
            int topPx = screenHeight / 2 - reactionSizePx / 2
                    - res.getDimensionPixelSize(R.dimen.toolbar_total_height);
            int rightPx = screenWidth - (leftPx - reactionSizePx);
            int bottomPx = screenHeight - (topPx - reactionSizePx);
            lp.setMargins(leftPx, topPx, rightPx, bottomPx);

            addReactionLayoutToScene(reactionLayout, lp);
        });
    }

    /**
     * Advances all reactions to the next frame. The given callback is invoked when all reactions
     * have decoded their next frame and are ready to be drawn.
     */
    public void stepReactions(Callback<Void> cb) {
        AtomicInteger expectedCallbacks = new AtomicInteger(mReactionLayouts.size());

        for (ReactionLayout rl : mReactionLayouts) {
            rl.getReaction().step(new Callback<Void>() {
                @Override
                public void onResult(Void v) {
                    if (expectedCallbacks.decrementAndGet() == 0) {
                        // The BaseGifDrawable class posts the result of the frame decoding to the
                        // UI thread. Post the callback back to a worker thread.
                        PostTask.postTask(
                                TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> { cb.onResult(null); });
                    }
                }
            });
        }
    }

    /**
     * Returns the frame count of the reaction with the most frames among those currently added to
     * the scene.
     */
    public int getFrameCount() {
        if (mReactionLayouts.size() == 0) {
            // If there are no reactions in the scene, return a frame count of 1 for the screenshot
            // background.
            return 1;
        }

        ReactionLayout maxFramesLayout =
                Collections.max(mReactionLayouts, new Comparator<ReactionLayout>() {
                    @Override
                    public int compare(ReactionLayout rl1, ReactionLayout rl2) {
                        return Integer.compare(rl1.getReaction().getMetadata().frameCount,
                                rl2.getReaction().getMetadata().frameCount);
                    }
                });

        return maxFramesLayout.getReaction().getMetadata().frameCount;
    }

    /**
     * Gets the width of the current scene, in pixels.
     */
    public int getWidth() {
        return mSceneBackground.getWidth();
    }

    /**
     * Gets the height of the current scene, in pixels.
     */
    public int getHeight() {
        return mSceneBackground.getHeight();
    }

    /**
     * Draws the scene view to the provided canvas.
     */
    public void drawScene(Canvas canvas) {
        mSceneBackground.draw(canvas);
    }

    /**
     * Deselects the active reaction, if any.
     */
    public void clearSelection() {
        markActiveStatus(mActiveReaction, false);
    }

    /**
     * Returns the total number of times (across all reactions) that the user added a new reaction
     * to the scene.
     */
    public int getNbReactionsAdded() {
        return mNbReactionsAdded;
    }

    /**
     * Returns the total number of times (across all reactions) that the user changed an existing
     * reaction's type.
     */
    public int getNbTypeChange() {
        return mNbTypeChange;
    }

    /**
     * Returns the total number of times (across all reactions) that the user scaled or rotated a
     * reaction.
     */
    public int getNbRotateScale() {
        return mNbRotateScale;
    }

    /**
     * Returns the total number of times (across all reactions) that the user duplicated a reaction.
     */
    public int getNbDuplicate() {
        return mNbDuplicate;
    }

    /**
     * Returns the total number of times (across all reactions) that the user deleted a reaction.
     */
    public int getNbDelete() {
        return mNbDelete;
    }

    /**
     * Returns the total number of times (across all reactions) that the user moved a reaction.
     */
    public int getNbMove() {
        return mNbMove;
    }

    // SceneEditorDelegate implementation.
    @Override
    public boolean canAddReaction() {
        return mReactionLayouts.size() < MAX_REACTION_COUNT;
    }

    @Override
    public void duplicateReaction(ReactionLayout reactionLayout) {
        ++mNbDuplicate;
        ReactionMetadata reaction = reactionLayout.getReaction().getMetadata();
        mMediator.getGifForUrl(reaction.assetUrl, (baseGifImage) -> {
            ReactionLayout newReactionLayout = (ReactionLayout) LayoutInflaterUtils.inflate(
                    mActivity, R.layout.reaction_layout, null);
            ReactionGifDrawable drawable =
                    new ReactionGifDrawable(reaction, baseGifImage, Bitmap.Config.ARGB_8888);
            newReactionLayout.init(drawable, this, reaction.localizedName);

            RelativeLayout.LayoutParams oldLayoutParams =
                    (RelativeLayout.LayoutParams) reactionLayout.getLayoutParams();
            RelativeLayout.LayoutParams newLayoutParams =
                    new RelativeLayout.LayoutParams(reactionLayout.getLayoutParams());
            int offsetPx = ViewUtils.dpToPx(mActivity, REACTION_OFFSET_DP);
            newLayoutParams.leftMargin = oldLayoutParams.leftMargin + offsetPx;
            newLayoutParams.topMargin = oldLayoutParams.topMargin + offsetPx;
            newLayoutParams.rightMargin = oldLayoutParams.rightMargin + offsetPx;
            newLayoutParams.bottomMargin = oldLayoutParams.bottomMargin + offsetPx;
            newReactionLayout.setRotation(reactionLayout.getRotation());

            if (isOutOfBoundsToTheBottomRight(newLayoutParams, reactionLayout.getRotation())) {
                newLayoutParams.leftMargin = oldLayoutParams.leftMargin - offsetPx;
                newLayoutParams.topMargin = oldLayoutParams.topMargin - offsetPx;
                newLayoutParams.rightMargin = oldLayoutParams.rightMargin - offsetPx;
                newLayoutParams.bottomMargin = oldLayoutParams.bottomMargin - offsetPx;
            }
            addReactionLayoutToScene(newReactionLayout, newLayoutParams);
        });
    }

    @Override
    public void removeReaction(ReactionLayout reactionLayout) {
        ++mNbDelete;
        markActiveStatus(reactionLayout, false);
        mSceneBackground.removeView(reactionLayout);
        mReactionLayouts.remove(reactionLayout);
    }

    @Override
    public void markActiveStatus(ReactionLayout reactionLayout, boolean isActive) {
        if (isActive) {
            if (mActiveReaction != null) {
                mActiveReaction.setActive(false);
            }
            reactionLayout.setActive(true);
            mActiveReaction = reactionLayout;
            mActiveReaction.bringToFront();
        } else if (mActiveReaction != null) {
            mActiveReaction.setActive(false);
            mActiveReaction = null;
        }
    }

    @Override
    public void reactionWasMoved() {
        ++mNbMove;
    }

    @Override
    public void reactionWasAdjusted() {
        ++mNbRotateScale;
    }

    // ToolbarReactionsDelegate implementation.
    @Override
    public void onToolbarReactionTapped(ReactionMetadata reaction) {
        if (mActiveReaction != null) {
            replaceActiveReaction(reaction);
        } else {
            addReactionInDefaultLocation(reaction);
        }
    }

    private void replaceActiveReaction(ReactionMetadata reaction) {
        assert mActiveReaction != null;
        ++mNbTypeChange;
        mMediator.getGifForUrl(reaction.assetUrl, (baseGifImage) -> {
            mActiveReaction.setDrawable(
                    new ReactionGifDrawable(reaction, baseGifImage, Bitmap.Config.ARGB_8888),
                    reaction.localizedName);
            resetReactions(mActiveReaction);
        });
    }

    private void addReactionLayoutToScene(
            ReactionLayout reactionLayout, RelativeLayout.LayoutParams layoutParams) {
        mSceneBackground.addView(reactionLayout, layoutParams);
        mReactionLayouts.add(reactionLayout);
        markActiveStatus(reactionLayout, true);
        resetReactions(reactionLayout);
    }

    private void resetReactions(ReactionLayout layoutToExclude) {
        for (ReactionLayout rl : mReactionLayouts) {
            if (rl != layoutToExclude) {
                rl.getReaction().resetAnimation();
            }
        }
    }

    /**
     * Starts all of the {@link ReactionLayout} animations on the scene.
     */
    public void startAnimations() {
        for (ReactionLayout rl : mReactionLayouts) {
            rl.getReaction().setSteppingEnabled(false);
            rl.getReaction().start();
        }
    }

    private boolean isOutOfBoundsToTheBottomRight(
            RelativeLayout.LayoutParams layoutParams, float rotation) {
        Resources res = mActivity.getResources();
        int buttonPadding = (int) Math.ceil(res.getDimensionPixelSize(R.dimen.button_size) / 2.0);
        int screenWidth = res.getDisplayMetrics().widthPixels - buttonPadding;
        int screenHeight = res.getDisplayMetrics().heightPixels
                - res.getDimensionPixelSize(R.dimen.toolbar_total_height) - buttonPadding;
        double centerX = layoutParams.leftMargin + layoutParams.width / 2.0;
        double centerY = layoutParams.topMargin + layoutParams.height / 2.0;
        double sin = Math.sin(Math.toRadians(rotation)) / 2;
        double cos = Math.cos(Math.toRadians(rotation)) / 2;

        double bottomRightX = centerX + layoutParams.width * cos - layoutParams.height * sin;
        double bottomRightY = centerY + layoutParams.width * sin + layoutParams.height * cos;
        double bottomLeftX = centerX - layoutParams.width * cos - layoutParams.height * sin;
        double bottomLeftY = centerY - layoutParams.width * sin + layoutParams.height * cos;
        double topRightX = centerX + layoutParams.width * cos + layoutParams.height * sin;
        double topRightY = centerY + layoutParams.width * sin - layoutParams.height * cos;
        double topLeftX = centerX - layoutParams.width * cos + layoutParams.height * sin;
        double topLeftY = centerY - layoutParams.width * sin - layoutParams.height * cos;

        return screenWidth < bottomRightX || screenHeight < bottomRightY
                || screenWidth < bottomLeftX || screenHeight < bottomLeftY
                || screenWidth < topRightX || screenHeight < topRightY || screenWidth < topLeftX
                || screenHeight < topLeftY;
    }
}
