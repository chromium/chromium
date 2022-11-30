// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.toolbar;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.components.content_creation.reactions.ReactionMetadata;
import org.chromium.ui.base.ViewUtils;

import java.util.List;

/**
 * Coordinator for the Lightweight Reactions toolbar.
 */
public class ToolbarCoordinator {
    private static final int THUMBNAIL_SIZE_DP = 56;

    private final ToolbarControlsDelegate mControlsDelegate;
    private final ToolbarReactionsDelegate mReactionsDelegate;
    private final RelativeLayout mRootLayout;

    public ToolbarCoordinator(View parentView, ToolbarControlsDelegate controlsDelegate,
            ToolbarReactionsDelegate reactionsDelegate) {
        mControlsDelegate = controlsDelegate;
        mReactionsDelegate = reactionsDelegate;

        mRootLayout = parentView.findViewById(R.id.lightweight_reactions_toolbar);

        View closeButton = mRootLayout.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> mControlsDelegate.cancelButtonTapped());
        View doneButton = mRootLayout.findViewById(R.id.done_button);
        doneButton.setOnClickListener(v -> mControlsDelegate.doneButtonTapped());
    }

    /**
     * Populates the reaction picker carousel with the given thumbnails.
     */
    public void initReactions(List<ReactionMetadata> availableReactions, Bitmap[] thumbnails) {
        assert availableReactions.size() == thumbnails.length;

        LinearLayout lr =
                mRootLayout.findViewById(R.id.lightweight_reactions_toolbar_reaction_picker);
        for (int i = 0; i < thumbnails.length; ++i) {
            ReactionMetadata reactionMetadata = availableReactions.get(i);
            ImageView iv = new ImageView(mRootLayout.getContext());
            iv.setImageBitmap(thumbnails[i]);
            int thumbnailSizePx = ViewUtils.dpToPx(mRootLayout.getContext(), THUMBNAIL_SIZE_DP);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    thumbnailSizePx, LinearLayout.LayoutParams.MATCH_PARENT);
            iv.setLayoutParams(params);
            iv.setAdjustViewBounds(true);
            iv.setScaleType(ImageView.ScaleType.CENTER_CROP);

            iv.setClickable(true);
            iv.setTag(reactionMetadata);
            iv.setOnClickListener(this::onReactionTapped);
            iv.setContentDescription(reactionMetadata.localizedName);

            lr.addView(iv);
        }
    }

    private void onReactionTapped(View view) {
        ImageView iv = (ImageView) view;
        ReactionMetadata rm = (ReactionMetadata) iv.getTag();
        mReactionsDelegate.onToolbarReactionTapped(rm);
    }
}
