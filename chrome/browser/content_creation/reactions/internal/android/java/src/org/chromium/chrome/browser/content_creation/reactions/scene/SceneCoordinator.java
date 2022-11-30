// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.scene;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.util.Size;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.content_creation.reactions.LightweightReactionsMediator;
import org.chromium.chrome.browser.content_creation.reactions.ReactionGifDrawable;
import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.chrome.browser.content_creation.reactions.toolbar.ToolbarReactionsDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.content_creation.reactions.ReactionMetadata;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Manages the scene UI and the reactions on the scene.
 */
public class SceneCoordinator implements SceneEditorDelegate, ToolbarReactionsDelegate {
    private static final int DEFAULT_REACTION_SIZE_DP = 192;
    private static final int REACTION_OFFSET_DP = 45;
    private static final int DEFAULT_MAX_REACTION_COUNT = 10;
    private static final String MAX_REACTIONS_PARAM_NAME = "max_reactions";

    private final Activity mActivity;
    private final LightweightReactionsMediator mMediator;
    private final Set<ReactionLayout> mReactionLayouts;
    private final Map<ReactionLayout, Integer> mInitialPositionByReaction;
    private final List<Integer> mNumReactionsInPosition;
    private final int mMaxReactionCount;

    private ReactionLayout mActiveReaction;
    private RelativeLayout mSceneView;
    private ImageView mScreenshotView;
    private Toast mToast;

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
        mInitialPositionByReaction = new HashMap<>();
        mNumReactionsInPosition = new ArrayList<>();

        mNbReactionsAdded = 0;
        mNbTypeChange = 0;
        mNbRotateScale = 0;
        mNbDuplicate = 0;
        mNbDelete = 0;
        mNbMove = 0;

        mMaxReactionCount = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.LIGHTWEIGHT_REACTIONS, MAX_REACTIONS_PARAM_NAME,
                DEFAULT_MAX_REACTION_COUNT);
    }

    public void setSceneViews(RelativeLayout sceneView, ImageView screenshotView) {
        mSceneView = sceneView;
        mSceneView.setOnClickListener((view) -> { clearSelection(); });
        mScreenshotView = screenshotView;
    }

    public void addReactionInDefaultLocation(ReactionMetadata reaction) {
        ++mNbReactionsAdded;
        mMediator.getGifForUrl(reaction.assetUrl, (baseGifImage) -> {
            if (mSceneView == null) {
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
            int offsetPx = ViewUtils.dpToPx(mActivity, REACTION_OFFSET_DP);
            int maxOffsetNum = Math.min((screenWidth - reactionSizePx) / 2 / offsetPx,
                    (screenHeight - reactionSizePx) / 2 / offsetPx);
            int offsetNum = Math.min(getFreePosition(), maxOffsetNum);
            int leftPx = screenWidth / 2 - reactionSizePx / 2 + offsetNum * offsetPx;
            int topPx = screenHeight / 2 - reactionSizePx / 2
                    - res.getDimensionPixelSize(R.dimen.toolbar_total_height)
                    + offsetNum * offsetPx;
            int rightPx = screenWidth - (leftPx - reactionSizePx);
            int bottomPx = screenHeight - (topPx - reactionSizePx);
            lp.setMargins(leftPx, topPx, rightPx, bottomPx);

            mInitialPositionByReaction.put(reactionLayout, offsetNum);
            if (offsetNum < mNumReactionsInPosition.size()) {
                mNumReactionsInPosition.set(offsetNum, mNumReactionsInPosition.get(offsetNum) + 1);
            } else {
                mNumReactionsInPosition.add(1);
            }
            addReactionLayoutToScene(reactionLayout, lp);
            reactionLayout.announceForAccessibility(mActivity.getString(
                    R.string.lightweight_reactions_reaction_added_announcement));
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
     * Gets the width of the scene view, in pixels.
     */
    public int getSceneWidth() {
        return mSceneView.getWidth();
    }

    /**
     * Gets the height of the scene view, in pixels.
     */
    public int getSceneHeight() {
        return mSceneView.getHeight();
    }

    /**
     * Gets the actual display dimensions of the screenshot image, in pixels.
     */
    public Size getScreenshotDisplaySize() {
        float[] imageMatrix = new float[9];
        mScreenshotView.getImageMatrix().getValues(imageMatrix);
        int intrinsicWidth = mScreenshotView.getDrawable().getIntrinsicWidth();
        int intrinsicHeight = mScreenshotView.getDrawable().getIntrinsicHeight();
        float scaleX = imageMatrix[Matrix.MSCALE_X];
        float scaleY = imageMatrix[Matrix.MSCALE_Y];
        int actualWidth = Math.round(intrinsicWidth * scaleX);
        int actualHeight = Math.round(intrinsicHeight * scaleY);
        return new Size(actualWidth, actualHeight);
    }

    /**
     * Draws the scene view to the provided canvas.
     */
    public void drawScene(Canvas canvas) {
        mSceneView.draw(canvas);
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

    /**
     * Returns the reactions currently in the scene.
     */
    public Set<ReactionLayout> getReactions() {
        return mReactionLayouts;
    }

    public void handleOrientationChange() {
        final float previousScreenshotViewX = mScreenshotView.getX();
        final float previousScreenshotViewWidth = mScreenshotView.getWidth();
        final float previousScreenshotImageWidth = getScreenshotDisplaySize().getWidth();
        final float previousScreenshotImageX = previousScreenshotViewX
                + (previousScreenshotViewWidth - previousScreenshotImageWidth) / 2;

        ViewTreeObserver vto = mSceneView.getViewTreeObserver();
        vto.addOnGlobalLayoutListener(new OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                mSceneView.getViewTreeObserver().removeOnGlobalLayoutListener(this);

                float screenshotViewX = mScreenshotView.getX();
                float screenshotViewWidth = mScreenshotView.getWidth();
                float screenshotImageWidth = getScreenshotDisplaySize().getWidth();
                float screenshotImageX =
                        screenshotViewX + (screenshotViewWidth - screenshotImageWidth) / 2;
                float screenshotImageRatio = screenshotImageWidth / previousScreenshotImageWidth;

                for (ReactionLayout rl : mReactionLayouts) {
                    // Normalize the reaction's X coordinate relative to the screenshot image's
                    // previous absolute X coordinate, and then multiply the resulting value by the
                    // screenshot ratio. This gives the new normalized X coordinate relative to the
                    // image's new X coordinate. Then, convert the normalized X back to its absolute
                    // value using the image's new absolute X coordinate.
                    float previousNormalizedX = rl.getX() - previousScreenshotImageX;
                    float newNormalizedX = previousNormalizedX * screenshotImageRatio;
                    rl.setX(newNormalizedX + screenshotImageX);

                    // The Y coordinate simply needs to be multiplied by the screenshot ratio,
                    // because the screenshot view and the actual image always have Y = 0.
                    rl.setY(rl.getY() * screenshotImageRatio);

                    // Finally, scale the reaction by the screenshot ratio.
                    RelativeLayout.LayoutParams layoutParams =
                            (RelativeLayout.LayoutParams) rl.getLayoutParams();
                    layoutParams.width = (int) (screenshotImageRatio * layoutParams.width);
                    layoutParams.height = (int) (screenshotImageRatio * layoutParams.height);
                    rl.setLayoutParams(layoutParams);
                }

                // Force a redraw of the scene to ensure the new dimensions and positions
                // are correctly displayed.
                mSceneView.invalidate();
            }
        });
    }

    // SceneEditorDelegate implementation.
    @Override
    public boolean canAddReaction() {
        return mReactionLayouts.size() < mMaxReactionCount;
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
            mActiveReaction.announceForAccessibility(mActivity.getString(
                    R.string.lightweight_reactions_reaction_duplicated_announcement));
        });
    }

    @Override
    public void showMaxReactionsReachedToast() {
        if (mToast != null) {
            mToast.cancel();
        }
        mToast = Toast.makeText(mActivity,
                mActivity.getString(R.string.lightweight_reactions_error_max_reactions_reached,
                        mMaxReactionCount),
                Toast.LENGTH_SHORT);
        mToast.show();
    }

    @Override
    public void removeReaction(ReactionLayout reactionLayout) {
        ++mNbDelete;
        markActiveStatus(reactionLayout, false);
        mSceneView.removeView(reactionLayout);
        removeReactionLayoutFromInitialPosition(reactionLayout);
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
    public void reactionWasMoved(ReactionLayout reactionLayout) {
        ++mNbMove;
        removeReactionLayoutFromInitialPosition(reactionLayout);
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
            if (canAddReaction()) {
                addReactionInDefaultLocation(reaction);
            } else {
                showMaxReactionsReachedToast();
            }
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
            mActiveReaction.announceForAccessibility(mActivity.getString(
                    R.string.lightweight_reactions_reaction_changed_announcement));
        });
    }

    private void addReactionLayoutToScene(
            ReactionLayout reactionLayout, RelativeLayout.LayoutParams layoutParams) {
        mSceneView.addView(reactionLayout, layoutParams);
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

    private void removeReactionLayoutFromInitialPosition(ReactionLayout reactionLayout) {
        Integer index = mInitialPositionByReaction.remove(reactionLayout);
        if (index != null) {
            mNumReactionsInPosition.set(index, mNumReactionsInPosition.get(index) - 1);
        }
    }

    private int getFreePosition() {
        for (int i = 0; i < mNumReactionsInPosition.size(); i++) {
            if (mNumReactionsInPosition.get(i) == 0) {
                return i;
            }
        }
        return mNumReactionsInPosition.size();
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
