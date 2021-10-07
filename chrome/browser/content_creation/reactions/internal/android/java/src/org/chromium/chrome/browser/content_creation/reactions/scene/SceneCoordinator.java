// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.scene;

import android.app.Activity;
import android.content.res.Resources;
import android.widget.RelativeLayout;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.base.ViewUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * Manages the scene UI and the reactions on the scene.
 */
public class SceneCoordinator implements SceneEditorDelegate {
    private static final int DEFAULT_REACTION_SIZE_DP = 100;
    private static final int REACTION_OFFSET_DP = 45;
    private static final int MAX_REACTION_COUNT = 10;

    private final Activity mActivity;
    private final Set<ReactionLayout> mReactionLayouts;

    private RelativeLayout mSceneBackground;

    /**
     * Constructs a new {@link SceneCoordinator}.
     *
     * @param activity The current {@link Activity}.
     */
    public SceneCoordinator(Activity activity) {
        mActivity = activity;
        mReactionLayouts = new HashSet<>();
    }

    public void setSceneBackground(RelativeLayout sceneBackground) {
        mSceneBackground = sceneBackground;
    }

    public void addInitialReaction() {
        if (mSceneBackground == null) {
            return;
        }
        assert mReactionLayouts.isEmpty();

        ReactionLayout reactionLayout = (ReactionLayout) LayoutInflaterUtils.inflate(
                mActivity, R.layout.reaction_layout, null);
        reactionLayout.init(
                AppCompatResources.getDrawable(mActivity, org.chromium.chrome.R.drawable.qr_code),
                this);

        int reactionSizePx = ViewUtils.dpToPx(mActivity, DEFAULT_REACTION_SIZE_DP);
        RelativeLayout.LayoutParams lp =
                new RelativeLayout.LayoutParams(reactionSizePx, reactionSizePx);
        Resources res = mActivity.getResources();
        int leftPx = res.getDisplayMetrics().widthPixels / 2 - reactionSizePx / 2;
        int topPx = res.getDisplayMetrics().heightPixels / 2 - reactionSizePx / 2
                - res.getDimensionPixelSize(R.dimen.toolbar_total_height);
        lp.setMargins(leftPx, topPx, 0, 0);

        mSceneBackground.addView(reactionLayout, lp);
        mReactionLayouts.add(reactionLayout);
    }

    // SceneEditorCallback implementation.
    @Override
    public boolean canAddReaction() {
        return mReactionLayouts.size() < MAX_REACTION_COUNT;
    }

    @Override
    public void duplicateReaction(ReactionLayout reactionLayout) {
        ReactionLayout newReactionLayout = (ReactionLayout) LayoutInflaterUtils.inflate(
                mActivity, R.layout.reaction_layout, null);
        newReactionLayout.init(reactionLayout.getReaction(), this);

        // TODO(crbug/1257738): Make sure the reaction is within bounds.
        RelativeLayout.LayoutParams oldLayoutParams =
                (RelativeLayout.LayoutParams) reactionLayout.getLayoutParams();
        RelativeLayout.LayoutParams newLayoutParams =
                new RelativeLayout.LayoutParams(reactionLayout.getLayoutParams());
        int offsetPx = ViewUtils.dpToPx(mActivity, REACTION_OFFSET_DP);
        newLayoutParams.leftMargin = oldLayoutParams.leftMargin + offsetPx;
        newLayoutParams.topMargin = oldLayoutParams.topMargin + offsetPx;
        newReactionLayout.setLayoutParams(newLayoutParams);

        mSceneBackground.addView(newReactionLayout);
        mReactionLayouts.add(newReactionLayout);
    }

    @Override
    public void removeReaction(ReactionLayout reactionLayout) {
        mSceneBackground.removeView(reactionLayout);
        mReactionLayouts.remove(reactionLayout);
    }

    @Override
    public void markActiveStatus(ReactionLayout reactionLayout, boolean isActive) {
        // no-op for now
    }
}
