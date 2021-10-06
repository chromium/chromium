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
    private static final int DEFAULT_REACTION_SIZE = 100;
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
        reactionLayout.setReaction(
                AppCompatResources.getDrawable(mActivity, org.chromium.chrome.R.drawable.qr_code));

        int reactionSizePx = ViewUtils.dpToPx(mActivity, DEFAULT_REACTION_SIZE);
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
        // TODO(crbug/1257326): Finish implementing duplicateReaction(), including offsetting
        ReactionLayout newReactionLayout = (ReactionLayout) LayoutInflaterUtils.inflate(
                mActivity, R.layout.reaction_layout, null);
        newReactionLayout.setReaction(reactionLayout.getReaction());
        newReactionLayout.setLayoutParams(reactionLayout.getLayoutParams());

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
