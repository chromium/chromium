// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AbstractListObserver;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * Coordinator responsible for suggesting chips to the user.
 */
public class AssistantSuggestionsCarouselCoordinator implements AssistantCarouselCoordinator {
    private final LinearLayoutManager mLayoutManager;
    private final RecyclerView mView;

    public AssistantSuggestionsCarouselCoordinator(Context context, AssistantCarouselModel model) {
        mLayoutManager = new LinearLayoutManager(
                context, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false);

        // Workaround for b/128679161.
        mLayoutManager.setMeasurementCacheEnabled(false);

        mView = new RecyclerView(context);
        mView.setLayoutManager(mLayoutManager);
        mView.addItemDecoration(new SpaceItemDecoration(context));
        mView.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model.getChipsModel(),
                        AssistantChipViewHolder::getViewType, AssistantChipViewHolder::bind),
                AssistantChipViewHolder::create));

        model.getChipsModel().addObserver(new AbstractListObserver<Void>() {
            @Override
            public void onDataSetChanged() {
                mView.invalidateItemDecorations();
            }
        });

        // Invalidate decorations when the width of the recycler view changes.
        mView.addOnLayoutChangeListener(
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    if (right - left != oldRight - oldLeft) {
                        // We post the invalidation because ItemDecoration::getItemOffsets is called
                        // before this listener when the size of the recycler view changes (when
                        // that happens, the width of the parent is still the old width), so calling
                        // mView.invalidateItemDecorations() directly will not do anything.
                        ThreadUtils.postOnUiThread(mView::invalidateItemDecorations);
                    }
                });
    }

    @Override
    public RecyclerView getView() {
        return mView;
    }

    private class SpaceItemDecoration extends RecyclerView.ItemDecoration {
        private final int mInnerSpacePx;
        private final int mOuterSpacePx;

        SpaceItemDecoration(Context context) {
            mInnerSpacePx = context.getResources().getDimensionPixelSize(
                                    R.dimen.autofill_assistant_suggestions_spacing)
                    / 2;
            mOuterSpacePx = context.getResources().getDimensionPixelSize(
                    R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            int position = parent.getChildAdapterPosition(view);

            // If old position != NO_POSITION, it means the carousel is being animated and we should
            // use that position in our logic.
            ViewHolder viewHolder = parent.getChildViewHolder(view);
            if (viewHolder != null && viewHolder.getOldPosition() != RecyclerView.NO_POSITION) {
                position = viewHolder.getOldPosition();
            }

            if (position == RecyclerView.NO_POSITION) {
                return;
            }

            int left;
            int right;
            if (position == 0) {
                left = mOuterSpacePx;
            } else {
                left = mInnerSpacePx;
            }

            // We use RecyclerView.State#getItemCount() as it returns the correct value when the
            // carousel is being animated.
            if (position == state.getItemCount() - 1) {
                right = mOuterSpacePx;
            } else {
                right = mInnerSpacePx;
            }

            if (!mLayoutManager.getReverseLayout()) {
                outRect.left = left;
                outRect.right = right;
            } else {
                outRect.left = right;
                outRect.right = left;
            }
        }
    }
}
